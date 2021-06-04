#include "SDL.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <assert.h>
#include <time.h>

#include "vm.h"
#include "video.h"

// 4k resolution 4096x2160 is 8,847,360
#define MAX_SCREEN_TEXTURE_PIXELS 8847360

static SDL_Window *window = NULL;
static int winWidth, winHeight;
static char winTitle[512];
static bool winFullscreen;
static SDL_Renderer *renderer = NULL;
static SDL_RendererInfo rendererInfo;
static SDL_Texture *intermediateTexture = NULL;
static SDL_Texture *upscaleTexture = NULL;
static int lastTextureUpscaleW = 0;
static int lastTextureUpscaleH = 0;

static uint32_t pixels[CHIP8_W * CHIP8_H];

static int SetupUpscaleTexture();

int VideoInit(const char *title, int winScale, bool fullscreen)
{
    winWidth = CHIP8_W * winScale;
    winHeight = CHIP8_H * winScale;
    winFullscreen = fullscreen;

    uint32_t flags = SDL_WINDOW_ALLOW_HIGHDPI;
    if (winFullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        SDL_ShowCursor(SDL_DISABLE);
    }
    window =
        SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         winWidth, winHeight, flags);
    if (!window) {
        fprintf(stderr, "Failed to create SDL Window! %s\n", SDL_GetError());
        return -1;
    }

    renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "Failed to create SDL Renderer! %s\n", SDL_GetError());
        return -1;
    }
    if (SDL_GetRendererInfo(renderer, &rendererInfo) != 0) {
        fprintf(stderr, "Unable to get SDL RendererInfo! %s\n", SDL_GetError());
        return -1;
    }

    if (SDL_RenderSetLogicalSize(renderer, CHIP8_W, CHIP8_H) != 0) {
        fprintf(stderr, "Failed to set the SDL Renderer logical size! %s\n",
                SDL_GetError());
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    intermediateTexture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_STREAMING, CHIP8_W, CHIP8_H);
    if (!intermediateTexture) {
        fprintf(stderr, "Failed to create intermediate SDL texture! %s\n",
                SDL_GetError());
        return -1;
    }

    if (SetupUpscaleTexture() != 0) {
        fprintf(stderr, "Failed to setup upscale texture!\n");
        return -1;
    }

    printf("Video module initialised!\n");

    return 0;
}

void VideoDestroy()
{
    if (upscaleTexture) {
        SDL_DestroyTexture(upscaleTexture);
    }
    if (intermediateTexture) {
        SDL_DestroyTexture(intermediateTexture);
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }

    printf("Video module destroyed!\n");
}

void VideoPresent()
{
    VMColorPalette palette;
    VMGetColorPalette(palette);
    uint8_t *display = VMGetDisplayPixels();
    int len = CHIP8_W * CHIP8_H;

    for (int pos = 0; pos < len; pos++) {
        int on = ((display[pos / 8] >> (7 - pos % 8)) & 1);
        pixels[pos] = palette[on];
    }
    SDL_UpdateTexture(intermediateTexture, NULL, pixels, 4 * CHIP8_W);

    // Set the clear color to a darker shade of the off color.
    // This ensures that the background blends more nicely when aspect
    // ratio correction is needed.
    uint32_t dark = palette[0];
    uint8_t r = ((dark >> 16) & 0xFF) >> 1;
    uint8_t g = ((dark >> 8) & 0xFF) >> 1;
    uint8_t b = (dark & 0xFF) >> 1;
    SDL_SetRenderDrawColor(renderer, r, g, b, 0xFF);

    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, upscaleTexture);
    SDL_RenderCopy(renderer, intermediateTexture, NULL, NULL);

    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderCopy(renderer, upscaleTexture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

int VideoGetRefreshRate()
{
    assert(window != NULL);

    int displayIndex = SDL_GetWindowDisplayIndex(window);
    SDL_DisplayMode mode;
    int refreshRate = 0;
    if (SDL_GetDesktopDisplayMode(displayIndex, &mode) == 0) {
        if (mode.refresh_rate > 0) {
            refreshRate = mode.refresh_rate;
        }
    }

    return refreshRate;
}

int VideoToggleFullscreen()
{
    winFullscreen = !winFullscreen;

    if (winFullscreen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_ShowCursor(SDL_DISABLE);
    } else {
        SDL_SetWindowFullscreen(window, false);
        SDL_SetWindowSize(window, winWidth, winHeight);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED);
        SDL_ShowCursor(SDL_ENABLE);
    }

    if (SetupUpscaleTexture() != 0) {
        fprintf(stderr, "Failed to create new upscale texture\n");
    }

    return 0;
}

void VideoScreenshot()
{
    char imagePath[300];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(imagePath, sizeof(imagePath),
             "CHIP8-%s_%d-%02d-%02dT%02d-%02d-%02d.png", "screenshot",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
             tm.tm_min, tm.tm_sec);

    SDL_SetRenderTarget(renderer, upscaleTexture);

    int w, h;
    if (SDL_QueryTexture(upscaleTexture, NULL, NULL, &w, &h) != 0) {
        fprintf(stderr, "Failed to query texture for screenshot! %s\n",
                SDL_GetError());
        return;
    }

    int comp = 3;
    int stride = w * comp;
    unsigned char *screenPixels = malloc(h * stride);
    if (!screenPixels) {
        fprintf(stderr, "Failed to get malloc pixel buffer for screenshot!\n");
        return;
    }

    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGB24,
                             screenPixels, stride) != 0) {
        fprintf(stderr, "Failed to read renderer pixels! %s\n", SDL_GetError());
        return;
    }

    SDL_SetRenderTarget(renderer, NULL);

    if (stbi_write_png(imagePath, w, h, comp, screenPixels, stride) == 0)
        fprintf(stderr, "Failed to save screenshot!\n");
    else
        printf("Screenshot saved to %s!\n", imagePath);

    free(screenPixels);
}

void VideoSetWindowTitle(const char *format, ...)
{
    memset(winTitle, 0, sizeof(winTitle));

    va_list args;
    va_start(args, format);
    int titleLen = vsnprintf(winTitle, sizeof(winTitle), format, args);
    va_end(args);

    winTitle[titleLen] = '\0';
    SDL_SetWindowTitle(window, winTitle);
}

static int LimitUpscaleTextureSize(int *widthUpscale, int *heightUpscale);
static int GetTextureUpscale(int *widthUpscale, int *heightUpscale);

static int SetupUpscaleTexture()
{
    int widthUpscale = 1, heightUpscale = 1;
    if (GetTextureUpscale(&widthUpscale, &heightUpscale) != 0) {
        return -1;
    }

    if (widthUpscale == lastTextureUpscaleW &&
        heightUpscale == lastTextureUpscaleH) {
        return 0;
    }

    printf(
        "Texture upscale has changed! Initialising the upscale texture...\n");

    int screenTextureW = CHIP8_W * widthUpscale;
    int screenTextureH = CHIP8_H * heightUpscale;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_Texture *newUpscaleTexture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_TARGET, screenTextureW,
                          screenTextureH);
    if (!newUpscaleTexture) {
        fprintf(stderr, "Failed to create upscale SDL texture: %s!\n",
                SDL_GetError());
        return -1;
    }

    // Destroy the upscaled texture if it already exists.
    if (upscaleTexture) {
        SDL_DestroyTexture(upscaleTexture);
    }

    upscaleTexture = newUpscaleTexture;
    lastTextureUpscaleW = screenTextureW;
    lastTextureUpscaleH = screenTextureH;

    printf("Upscale texture initialized! Size: %dx%d, upscale: %dx%d\n",
           screenTextureW, screenTextureH, widthUpscale, heightUpscale);
    return 0;
}

static int LimitUpscaleTextureSize(int *widthUpscale, int *heightUpscale)
{
    int maxTextureWidth = rendererInfo.max_texture_width;
    int maxTextureHeight = rendererInfo.max_texture_height;

    while (*widthUpscale * CHIP8_W > maxTextureWidth) {
        --*widthUpscale;
    }
    while (*heightUpscale * CHIP8_H > maxTextureHeight) {
        --*heightUpscale;
    }

    if ((*widthUpscale < 1 && maxTextureWidth > 0) ||
        (*heightUpscale < 1 && maxTextureHeight > 0)) {
        fprintf(
            stderr,
            "Unable to create a texture big enough for the whole screen! Maximum texture size %dx%d\n",
            maxTextureWidth, maxTextureWidth);
        return -1;
    }

    // We limit the amount of texture memory used for the screen texture,
    // since beyond a certain point there are diminishing returns. Also,
    // depending on the hardware there may be performance problems with very
    // huge textures.
    while (*widthUpscale * *heightUpscale * CHIP8_W * CHIP8_H >
           MAX_SCREEN_TEXTURE_PIXELS) {
        if (*widthUpscale > *heightUpscale) {
            --*widthUpscale;
        } else {
            --*heightUpscale;
        }
    }

    return 0;
}

static int GetTextureUpscale(int *widthUpscale, int *heightUpscale)
{
    int w, h;
    if (SDL_GetRendererOutputSize(renderer, &w, &h) != 0) {
        fprintf(stderr, "Failed to get the renderer output size: %s\n",
                SDL_GetError());
        return -1;
    }

    if (w > h) {
        // Wide window.
        w = h * (CHIP8_W / CHIP8_H);
    } else {
        // Tall window.
        h = w * (CHIP8_H / CHIP8_W);
    }

    *widthUpscale = (w + CHIP8_W - 1) / CHIP8_W;
    *heightUpscale = (h + CHIP8_H - 1) / CHIP8_H;

    if (*widthUpscale < 1)
        *widthUpscale = 1;
    if (*heightUpscale < 1)
        *heightUpscale = 1;

    return LimitUpscaleTextureSize(widthUpscale, heightUpscale);
}
