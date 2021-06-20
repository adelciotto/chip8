#include "SDL.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <time.h>

#include "def.h"
#include "options.h"
#include "vm.h"

static void PollEvents();
static void ExitHandler();

static inline int64_t GetPerformanceCounter()
{
    return (int64_t)SDL_GetPerformanceCounter();
}

static inline int64_t GetPerformanceFrequency()
{
    return (int64_t)SDL_GetPerformanceFrequency();
}

static int64_t globalPerformanceFreq;

static inline double GetElapsedSeconds(uint64_t start, uint64_t end)
{
    return (double)(end - start) / (double)globalPerformanceFreq;
}

//////////////////// BEGIN VIDEO INTERFACE ////////////////////

struct Window {
    char title[512];
    int width;
    int height;
    bool fullscreen;
    bool closeRequested;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_RendererInfo rendererInfo;
};

struct StreamingTexture {
    int width;
    int height;
    int stride;
    SDL_Texture *texture;
    uint32_t *pixels;
};

struct UpscaleTexture {
    int width;
    int height;
    SDL_Texture *texture;
    int lastUpscaleX;
    int lastUpscaleY;
};

static bool InitVideo(int windowScale, bool fullscreen);
static void DestroyVideo();
static void PresentVideo();
static void ToggleFullscreen();
void SetWindowTitle(const char *format, ...);
static void CaptureScreenshot();

static struct Window globalWindow = { .title = "CHIP-8",
                                      .width = 0,
                                      .height = 0,
                                      .fullscreen = false,
                                      .window = NULL,
                                      .renderer = NULL };
static struct StreamingTexture globalStreamingTexture = { .width = 0,
                                                          .height = 0,
                                                          .stride = 0,
                                                          .texture = NULL,
                                                          .pixels = NULL };
static struct UpscaleTexture globalUpscaleTexture = { .width = 0,
                                                      .height = 0,
                                                      .texture = NULL,
                                                      .lastUpscaleX = 0,
                                                      .lastUpscaleY = 0 };

//////////////////// END VIDEO INTERFACE ////////////////////

//////////////////// BEGIN AUDIO INTERFACE ////////////////////

struct AudioDevice {
    SDL_AudioDeviceID ID;
    int sampleCount;
    int latencySampleCount;
    void *audioBuffer;
    int audioBufferLen;
    uint32_t runningSampleIndex;
};

static bool InitAudio();
static void DestroyAudio();
static void PushAudio();

static struct AudioDevice globalAudioDevice = { .ID = 0,
                                                .sampleCount = 0,
                                                .latencySampleCount = 0,
                                                .audioBuffer = NULL,
                                                .audioBufferLen = 0 };

//////////////////// END AUDIO INTERFACE ////////////////////

//////////////////// BEGIN MAIN ENTRY POINT ////////////////////

#define DELTA_TIME_HISTORY_COUNT 4

int main(int argc, char *argv[])
{
    Options options;
    OptionsCreateFromArgv(&options, argc, argv);

    printf("Option 'window_scale' set to %d\n", options.windowScale);
    printf("Option 'fullscreen' set to %d\n", options.fullscreen);
    printf("Option 'rom_path' set to %s\n", options.romPath);
    printf("Option 'cycles' set to %d\n", options.cyclesPerTick);
    printf("Option 'palette' set to %s\n", options.selectedPaletteName);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Failed to init SDL! %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    if (!InitVideo(options.windowScale, options.fullscreen)) {
        return EXIT_FAILURE;
    }

    if (!InitAudio()) {
        return EXIT_FAILURE;
    }

    VMInit(options.cyclesPerTick, options.palette);
    if (VMLoadRom(options.romPath) != 0) {
        fprintf(stderr, "Failed to load CHIP-8 rom %s!\n", options.romPath);
        ExitHandler();
        return EXIT_FAILURE;
    }

    // Credit to TylerGlaiel for the frame timing code that was used as a
    // reference.
    // https://github.com/TylerGlaiel/FrameTimingControl
    globalPerformanceFreq = GetPerformanceFrequency();
    int64_t targetTimePerTick = globalPerformanceFreq / VM_TICK_FREQUENCY;
    int64_t tickAccumulator = 0;
    int64_t lastCounter = GetPerformanceCounter();
    int64_t lastMetricsUpdateCounter = GetPerformanceCounter();

    int64_t vsyncMaxErr = globalPerformanceFreq * 0.0002;
    int64_t time60Hz = globalPerformanceFreq / 60;
    // clang-format off
    int64_t snapFrequencies[] = {
        time60Hz,           // 60fps
        time60Hz * 2,       // 30fps
        time60Hz * 3,       // 20fps
        time60Hz * 4,       // 15fps
        (time60Hz + 1) / 2  // 120ps
    };
    // clang-format on
    int64_t deltaTimeHistory[DELTA_TIME_HISTORY_COUNT];
    for (int i = 0; i < DELTA_TIME_HISTORY_COUNT; i++)
        deltaTimeHistory[i] = targetTimePerTick;
    unsigned int historyIndx = 0;

    while (!globalWindow.closeRequested) {
        int64_t currentCounter = GetPerformanceCounter();
        int64_t deltaTime = currentCounter - lastCounter;
        lastCounter = currentCounter;

        // Handle unexpected delta time anomalies.
        if (deltaTime > targetTimePerTick * 8) {
            deltaTime = targetTimePerTick;
        }
        if (deltaTime < 0) {
            deltaTime = 0;
        }

        // VSync time snapping.
        for (int i = 0; i < ARRAY_LEN(snapFrequencies); i++) {
            if (llabs(deltaTime - snapFrequencies[i]) < vsyncMaxErr) {
                deltaTime = snapFrequencies[i];
                break;
            }
        }

        // Average the delta time.
        deltaTimeHistory[historyIndx] = deltaTime;
        historyIndx = (historyIndx + 1) % DELTA_TIME_HISTORY_COUNT;
        deltaTime = 0;
        for (int i = 0; i < DELTA_TIME_HISTORY_COUNT; i++) {
            deltaTime += deltaTimeHistory[i];
        }
        deltaTime /= DELTA_TIME_HISTORY_COUNT;

        tickAccumulator += deltaTime;

        // Spiral of death protection.
        if (tickAccumulator > targetTimePerTick * 8) {
            tickAccumulator = 0;
            deltaTime = targetTimePerTick;
        }

        PollEvents();

        // Ensure the main application logic ticks at the correct frequency
        while (tickAccumulator >= targetTimePerTick) {
            VMTick();

            PushAudio();

            tickAccumulator -= targetTimePerTick;
        }

        PresentVideo();

        if (GetElapsedSeconds(lastMetricsUpdateCounter,
                              GetPerformanceCounter()) > 1.0) {
            double msPerFrame = (((1000.0 * (double)deltaTime) /
                                  (double)globalPerformanceFreq));
            int64_t fps = globalPerformanceFreq / deltaTime;
            SetWindowTitle("CHIP-8 | %.02fms/f, %d FPS", msPerFrame, fps);
            lastMetricsUpdateCounter = GetPerformanceCounter();
        }
    }

    ExitHandler();

    return EXIT_SUCCESS;
}

//////////////////// END MAIN ENTRY POINT ////////////////////

//////////////////// START EVENTS IMPLEMENTATION ////////////////////

static void KeyboardEventHandler(SDL_KeyboardEvent *event);
static void WindowEventHandler(SDL_WindowEvent *event);

static void PollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            globalWindow.closeRequested = true;
            break;
        case SDL_WINDOWEVENT:
            WindowEventHandler(&event.window);
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            KeyboardEventHandler(&event.key);
            break;
        }
    }
}

static bool CheckFullscreenToggle(SDL_Keysym sym);
static bool CheckScreenshot(SDL_Keysym sym);
static int MapKey(SDL_Scancode sc);

static void KeyboardEventHandler(SDL_KeyboardEvent *event)
{
    switch (event->type) {
    case SDL_KEYDOWN: {
        if (CheckFullscreenToggle(event->keysym)) {
            VMTogglePause(true);
            ToggleFullscreen();
            VMTogglePause(false);
            return;
        }

        if (CheckScreenshot(event->keysym)) {
            VMTogglePause(true);
            CaptureScreenshot();
            VMTogglePause(false);
            return;
        }

        int key = MapKey(event->keysym.sym);
        if (key >= 0) {
            VMSetKey(key);
        }
    } break;
    case SDL_KEYUP: {
        int key = MapKey(event->keysym.sym);
        if (key >= 0) {
            VMClearKey(key);
        }
    } break;
    }
}

static void WindowEventHandler(SDL_WindowEvent *event)
{
    switch (event->type) {
    case SDL_WINDOWEVENT_SHOWN:
    case SDL_WINDOWEVENT_FOCUS_GAINED:
        VMTogglePause(false);
        break;
    case SDL_WINDOWEVENT_HIDDEN:
    case SDL_WINDOWEVENT_FOCUS_LOST:
        VMTogglePause(true);
        break;
    }
}

static void ExitHandler()
{
    DestroyAudio();

    DestroyVideo();

    SDL_Quit();

    printf("All SDL resources destroyed\n");
}

static bool CheckFullscreenToggle(SDL_Keysym sym)
{
    uint16_t flags = (KMOD_LALT | KMOD_RALT);
#if defined(__MACOSX__)
    flags |= (KMOD_LGUI | KMOD_RGUI);
#endif
    return (sym.scancode == SDL_SCANCODE_RETURN ||
            sym.scancode == SDL_SCANCODE_KP_ENTER) &&
           (sym.mod & flags) != 0;
}

static bool CheckScreenshot(SDL_Keysym sym)
{
    return (sym.sym == SDLK_PRINTSCREEN);
}

static int MapKey(SDL_Scancode sc)
{
    // clang-format off
    switch (sc) {
    case SDLK_1: return 0x01;
    case SDLK_2: return 0x02;
    case SDLK_3: return 0x03;
    case SDLK_4: return 0x0C;
    case SDLK_q: return 0x04;
    case SDLK_w: return 0x05;
    case SDLK_e: return 0x06;
    case SDLK_r: return 0x0D;
    case SDLK_a: return 0x07;
    case SDLK_s: return 0x08;
    case SDLK_d: return 0x09;
    case SDLK_f: return 0x0E;
    case SDLK_z: return 0x0A;
    case SDLK_x: return 0x00;
    case SDLK_c: return 0x0B;
    case SDLK_v: return 0x0F;
    default: return -1;
    }
    // clang-format on
}

//////////////////// END EVENTS IMPLEMENTATION ////////////////////

//////////////////// BEGIN VIDEO IMPLEMENTATION ////////////////////

static bool InitWindowAndRenderer();
static bool InitStreamingTexture();
static bool InitUpscaleTexture();

static bool InitVideo(int windowScale, bool fullscreen)
{
    assert(windowScale > 0);

    globalWindow.width = CHIP8_W * windowScale;
    globalWindow.height = CHIP8_H * windowScale;
    globalWindow.fullscreen = fullscreen;

    if (!InitWindowAndRenderer()) {
        fprintf(stderr, "Failed to initialize SDL window and renderer!\n");
        return false;
    }

    if (!InitStreamingTexture()) {
        fprintf(stderr, "Failed to create streaming SDL texture! %s\n",
                SDL_GetError());
        return false;
    }

    if (!InitUpscaleTexture()) {
        fprintf(stderr, "Failed to setup upscale SDL texture! %s\n",
                SDL_GetError());
        return false;
    }

    return true;
}

static void DestroyVideo()
{
    if (globalUpscaleTexture.texture) {
        SDL_DestroyTexture(globalUpscaleTexture.texture);
    }
    if (globalStreamingTexture.texture) {
        SDL_DestroyTexture(globalStreamingTexture.texture);
    }
    if (globalStreamingTexture.pixels) {
        free(globalStreamingTexture.pixels);
    }
    if (globalWindow.renderer) {
        SDL_DestroyRenderer(globalWindow.renderer);
    }
    if (globalWindow.window) {
        SDL_DestroyWindow(globalWindow.window);
    }

    printf("All video resources destroyed\n");
}

static void PresentVideo()
{
    VMColorPalette palette;
    VMGetColorPalette(palette);
    uint8_t *display = VMGetDisplayPixels();
    int len = CHIP8_W * CHIP8_H;

    for (int pos = 0; pos < len; pos++) {
        int on = ((display[pos / 8] >> (7 - pos % 8)) & 1);
        globalStreamingTexture.pixels[pos] = palette[on];
    }
    SDL_UpdateTexture(globalStreamingTexture.texture, NULL,
                      globalStreamingTexture.pixels,
                      globalStreamingTexture.stride);

    // Set the clear color to a darker shade of the off color.
    // This ensures that the background blends more nicely when aspect
    // ratio correction is needed.
    uint32_t dark = palette[0];
    uint8_t r = ((dark >> 16) & 0xFF) >> 1;
    uint8_t g = ((dark >> 8) & 0xFF) >> 1;
    uint8_t b = (dark & 0xFF) >> 1;
    SDL_SetRenderDrawColor(globalWindow.renderer, r, g, b, 0xFF);

    SDL_RenderClear(globalWindow.renderer);
    SDL_SetRenderTarget(globalWindow.renderer, globalUpscaleTexture.texture);
    SDL_RenderCopy(globalWindow.renderer, globalStreamingTexture.texture, NULL,
                   NULL);

    SDL_SetRenderTarget(globalWindow.renderer, NULL);
    SDL_RenderCopy(globalWindow.renderer, globalUpscaleTexture.texture, NULL,
                   NULL);
    SDL_RenderPresent(globalWindow.renderer);
}

static void ToggleFullscreen()
{
    globalWindow.fullscreen = !globalWindow.fullscreen;

    if (globalWindow.fullscreen) {
        SDL_SetWindowFullscreen(globalWindow.window,
                                SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_ShowCursor(SDL_DISABLE);
    } else {
        SDL_SetWindowFullscreen(globalWindow.window, false);
        SDL_SetWindowSize(globalWindow.window, globalWindow.width,
                          globalWindow.height);
        SDL_SetWindowPosition(globalWindow.window, SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED);
        SDL_ShowCursor(SDL_ENABLE);
    }

    if (!InitUpscaleTexture()) {
        fprintf(stderr, "Failed to create new upscale texture\n");
    }
}

void SetWindowTitle(const char *format, ...)
{
    memset(globalWindow.title, 0, sizeof(globalWindow.title));

    va_list args;
    va_start(args, format);
    int titleLen =
        vsnprintf(globalWindow.title, sizeof(globalWindow.title), format, args);
    va_end(args);

    globalWindow.title[titleLen] = '\0';
    SDL_SetWindowTitle(globalWindow.window, globalWindow.title);
}

static void CaptureScreenshot()
{
    char imagePath[300];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(imagePath, sizeof(imagePath),
             "CHIP8-%s_%d-%02d-%02dT%02d-%02d-%02d.png", "screenshot",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
             tm.tm_min, tm.tm_sec);

    SDL_SetRenderTarget(globalWindow.renderer, globalUpscaleTexture.texture);

    int w = globalUpscaleTexture.width;
    int h = globalUpscaleTexture.height;
    int comp = 3;
    int stride = w * comp;
    unsigned char *screenPixels = malloc(h * stride);
    if (!screenPixels) {
        fprintf(stderr, "Failed to get malloc pixel buffer for screenshot!\n");
        return;
    }

    if (SDL_RenderReadPixels(globalWindow.renderer, NULL, SDL_PIXELFORMAT_RGB24,
                             screenPixels, stride) != 0) {
        fprintf(stderr, "Failed to read renderer pixels! %s\n", SDL_GetError());
        return;
    }

    SDL_SetRenderTarget(globalWindow.renderer, NULL);

    if (stbi_write_png(imagePath, w, h, comp, screenPixels, stride) == 0)
        fprintf(stderr, "Failed to save screenshot!\n");
    else
        printf("Screenshot saved to %s!\n", imagePath);

    free(screenPixels);
}

static bool InitWindowAndRenderer()
{
    uint32_t flags = SDL_WINDOW_ALLOW_HIGHDPI;
    if (globalWindow.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        SDL_ShowCursor(SDL_DISABLE);
    }
    globalWindow.window =
        SDL_CreateWindow(globalWindow.title, SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, globalWindow.width,
                         globalWindow.height, flags);
    if (!globalWindow.window) {
        fprintf(stderr, "Failed to create SDL Window! %s\n", SDL_GetError());
        return false;
    }

    globalWindow.renderer = SDL_CreateRenderer(globalWindow.window, -1,
                                               SDL_RENDERER_PRESENTVSYNC |
                                                   SDL_RENDERER_ACCELERATED);
    if (!globalWindow.renderer) {
        fprintf(stderr, "Failed to create SDL Renderer! %s\n", SDL_GetError());
        return false;
    }
    if (SDL_GetRendererInfo(globalWindow.renderer,
                            &globalWindow.rendererInfo) != 0) {
        fprintf(stderr, "Unable to get SDL RendererInfo! %s\n", SDL_GetError());
        return false;
    }

    if (SDL_RenderSetLogicalSize(globalWindow.renderer, CHIP8_W, CHIP8_H) !=
        0) {
        fprintf(stderr, "Failed to set the SDL Renderer logical size! %s\n",
                SDL_GetError());
    }

    return true;
}

static bool InitStreamingTexture()
{
    globalStreamingTexture.pixels = calloc(CHIP8_W * CHIP8_H, 4);
    if (!globalStreamingTexture.pixels) {
        fprintf(stderr, "Failed to allocate buffer for streaming texture!\n");
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    globalStreamingTexture.texture =
        SDL_CreateTexture(globalWindow.renderer, SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_STREAMING, CHIP8_W, CHIP8_H);
    if (!globalStreamingTexture.texture) {
        return false;
    }

    globalStreamingTexture.width = CHIP8_W;
    globalStreamingTexture.height = CHIP8_H;
    globalStreamingTexture.stride = CHIP8_W * 4;

    return true;
}

static bool LimitUpscaleTextureSize(int *upscaleX, int *upscaleY);
static bool GetTextureUpscale(int *upscaleX, int *upscaleY);

static bool InitUpscaleTexture()
{
    int upscaleX = 1, upscaleY = 1;
    if (!GetTextureUpscale(&upscaleX, &upscaleY)) {
        return false;
    }

    if (upscaleX == globalUpscaleTexture.lastUpscaleX &&
        upscaleY == globalUpscaleTexture.lastUpscaleY) {
        return true;
    }

    printf(
        "Texture upscale has changed! Initialising the upscale texture...\n");

    int textureWidth = CHIP8_W * upscaleX;
    int textureHeight = CHIP8_H * upscaleY;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_Texture *newUpscaleTexture =
        SDL_CreateTexture(globalWindow.renderer, SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_TARGET, textureWidth,
                          textureHeight);
    if (!newUpscaleTexture) {
        fprintf(stderr, "Failed to create upscale SDL texture: %s!\n",
                SDL_GetError());
        return false;
    }

    // Destroy the upscaled texture if it already exists.
    if (globalUpscaleTexture.texture) {
        SDL_DestroyTexture(globalUpscaleTexture.texture);
    }

    globalUpscaleTexture.texture = newUpscaleTexture;
    globalUpscaleTexture.width = textureWidth;
    globalUpscaleTexture.height = textureHeight;
    globalUpscaleTexture.lastUpscaleX = upscaleX;
    globalUpscaleTexture.lastUpscaleY = upscaleY;

    printf("Upscale texture initialized! Size: %dx%d, upscale: %dx%d\n",
           textureWidth, textureHeight, upscaleX, upscaleY);

    return true;
}

static bool GetTextureUpscale(int *upscaleX, int *upscaleY)
{
    int w, h;
    if (SDL_GetRendererOutputSize(globalWindow.renderer, &w, &h) != 0) {
        fprintf(stderr, "Failed to get the renderer output size: %s\n",
                SDL_GetError());
        return false;
    }

    if (w > h) {
        // Wide window.
        w = h * (CHIP8_W / CHIP8_H);
    } else {
        // Tall window.
        h = w * (CHIP8_H / CHIP8_W);
    }

    *upscaleX = (w + CHIP8_W - 1) / CHIP8_W;
    *upscaleY = (h + CHIP8_H - 1) / CHIP8_H;

    if (*upscaleX < 1)
        *upscaleX = 1;
    if (*upscaleY < 1)
        *upscaleY = 1;

    return LimitUpscaleTextureSize(upscaleX, upscaleY);
}

// 4k resolution 4096x2160 is 8,847,360
#define MAX_SCREEN_TEXTURE_PIXELS 8847360

static bool LimitUpscaleTextureSize(int *upscaleX, int *upscaleY)
{
    int maxTextureWidth = globalWindow.rendererInfo.max_texture_width;
    int maxTextureHeight = globalWindow.rendererInfo.max_texture_height;

    while (*upscaleX * CHIP8_W > maxTextureWidth) {
        --*upscaleX;
    }
    while (*upscaleY * CHIP8_H > maxTextureHeight) {
        --*upscaleY;
    }

    if ((*upscaleX < 1 && maxTextureWidth > 0) ||
        (*upscaleY < 1 && maxTextureHeight > 0)) {
        fprintf(
            stderr,
            "Unable to create a texture big enough for the whole screen! Maximum texture size %dx%d\n",
            maxTextureWidth, maxTextureWidth);
        return false;
    }

    // We limit the amount of texture memory used for the screen texture,
    // since beyond a certain point there are diminishing returns. Also,
    // depending on the hardware there may be performance problems with very
    // huge textures.
    while (*upscaleX * *upscaleY * CHIP8_W * CHIP8_H >
           MAX_SCREEN_TEXTURE_PIXELS) {
        if (*upscaleX > *upscaleY) {
            --*upscaleX;
        } else {
            --*upscaleY;
        }
    }

    return true;
}

//////////////////// END VIDEO IMPLEMENTATION ////////////////////

//////////////////// START AUDIO IMPLEMENTATION ////////////////////

static bool InitAudio()
{
    if (SDL_GetNumAudioDevices(0) <= 0) {
        fprintf(stderr, "No audio devices found!\n");
        return false;
    }

    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = 48000;
    want.format = AUDIO_S16LSB;
    want.channels = 1;
    // The VM ticks at 60hz. So use this frequency to calculate how many samples
    // are required per frame.
    want.samples = want.freq * 1 / VM_TICK_FREQUENCY;

    globalAudioDevice.ID = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (globalAudioDevice.ID == 0) {
        fprintf(stderr, "Failed to open SDL audio device! %s\n",
                SDL_GetError());
        return false;
    }

    globalAudioDevice.sampleCount = have.samples * have.channels;
    globalAudioDevice.latencySampleCount = have.freq / 15;
    globalAudioDevice.audioBuffer =
        calloc(globalAudioDevice.latencySampleCount, 2);
    if (!globalAudioDevice.audioBuffer) {
        fprintf(stderr,
                "Failed to allocate memory for audio buffer! size needed: %d\n",
                globalAudioDevice.latencySampleCount * 2);
        return false;
    }
    globalAudioDevice.audioBufferLen = globalAudioDevice.latencySampleCount * 2;

    SDL_PauseAudioDevice(globalAudioDevice.ID, 0);

    printf(
        "Sound module initialised! freq/f: %d, latency samples: %d, buffer size: %d\n",
        globalAudioDevice.sampleCount, globalAudioDevice.latencySampleCount,
        globalAudioDevice.audioBufferLen);

    return true;
}

static void DestroyAudio()
{
    if (globalAudioDevice.audioBuffer) {
        free(globalAudioDevice.audioBuffer);
    }
    if (globalAudioDevice.ID > 0) {
        SDL_CloseAudioDevice(globalAudioDevice.ID);
        globalAudioDevice.ID = 0;
    }

    printf("All audio resources destroyed\n");
}

static void PushAudio()
{
    if (globalAudioDevice.ID == 0 || VMGetSoundTimer() <= 0) {
        return;
    }

    int halfSquareWavePeriod = (48000 / 256) / 2;
    int toneVolume = 3000;
    int16_t *buffer = (int16_t *)globalAudioDevice.audioBuffer;
    int sampleCount = globalAudioDevice.latencySampleCount -
                      SDL_GetQueuedAudioSize(globalAudioDevice.ID) / 2;

    for (int i = 0; i < sampleCount; i++) {
        int16_t volume =
            (++globalAudioDevice.runningSampleIndex / halfSquareWavePeriod) % 2;
        buffer[i] = volume ? toneVolume : -toneVolume;
    }

    SDL_QueueAudio(globalAudioDevice.ID, buffer, sampleCount * 2);
}

//////////////////// END AUDIO IMPLEMENTATION ////////////////////
