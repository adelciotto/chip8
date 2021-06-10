#include "SDL.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "vm.h"

static int optionWinScale = 8;
static bool optionFullscreen = false;
static const char *optionRomPath = "test_opcode.ch8";
static int optionCycles = 20;
static VMColorPaletteType optionPalette = VMCOLOR_PALETTE_NOKIA;

static int refreshRate = 0;
static const char *selectedPaletteName = "nokia";

static void AtExit(void);
static void ParseCommandArgs(int argc, char *argv[]);

static uint64_t GetPerformanceCounter()
{
    return SDL_GetPerformanceCounter();
}

static double GetElapsedSeconds(uint64_t end, uint64_t start)
{
    return (double)(end - start) / (double)SDL_GetPerformanceFrequency();
}

int main(int argc, char *argv[])
{
    atexit(AtExit);

    ParseCommandArgs(argc, argv);
    printf("Option 'win_scale' set to %d\n", optionWinScale);
    printf("Option 'fullscreen' set to %d\n", optionFullscreen);
    printf("Option 'rom_path' set to %s\n", optionRomPath);
    printf("Option 'cycles' set to %d\n", optionCycles);
    printf("Option 'palette' set to %s\n", selectedPaletteName);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Failed to init SDL! %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    if (VideoInit("CHIP-8", optionWinScale, optionFullscreen) != 0) {
        fprintf(stderr, "Failed to init video!\n");
        exit(EXIT_FAILURE);
    }
    refreshRate = VideoGetRefreshRate();

    if (SoundInit() != 0) {
        fprintf(stderr, "Failed to init audio!\n");
        exit(EXIT_SUCCESS);
    }

    // TODO: CHIP8 uses LUT for instructions.
    int targetFreq = optionCycles * 60;
    int cyclesPerTic = MAX(targetFreq / refreshRate, 1);
    VMInit(cyclesPerTic, optionPalette);
    printf("VM clock rate: %dhz, cycles/tic: %d, tics/sec: %d\n", targetFreq,
           cyclesPerTic, refreshRate);

    if (VMLoadRom(optionRomPath) != 0) {
        fprintf(stderr, "Failed to load CHIP-8 rom %s!\n", optionRomPath);
        exit(EXIT_SUCCESS);
    }

    double targetSecsPerFrame = 1.0 / (double)refreshRate;
    uint64_t lastCounter = 0, metricsCounter = 0;
    uint64_t perfFreq = SDL_GetPerformanceFrequency();

    while (!InputCloseRequested()) {
        InputPollEvents();

        VMTic(targetSecsPerFrame);

        SoundPlay();

        // Cap the FPS to monitor refresh rate.
        if (lastCounter == 0) {
            lastCounter = GetPerformanceCounter();
            metricsCounter = GetPerformanceCounter();
        }
        if (GetElapsedSeconds(GetPerformanceCounter(), lastCounter) <
            targetSecsPerFrame) {
            double elapsed =
                GetElapsedSeconds(SDL_GetPerformanceCounter(), lastCounter);
            int32_t timeToSleep =
                (int32_t)((targetSecsPerFrame - elapsed) * 1000.0) - 1;
            if (timeToSleep > 0) {
                SDL_Delay(timeToSleep);
            }
            // assert(GetElapsedSeconds(GetPerformanceCounter(), lastCounter) <
            //        targetSecsPerFrame);
            while (GetElapsedSeconds(GetPerformanceCounter(), lastCounter) <
                   targetSecsPerFrame) {
                // Waiting...
            }
        }

        uint64_t endCounter = GetPerformanceCounter();

        VideoPresent();

        if (GetElapsedSeconds(GetPerformanceCounter(), metricsCounter) > 1.0) {
            uint64_t counterElapsed = endCounter - lastCounter;
            double msPerFrame =
                (((1000.0 * (double)counterElapsed) / (double)perfFreq));
            double fps = (double)perfFreq / (double)counterElapsed;
            VideoSetWindowTitle("CHIP-8 | %.02fms/f, %.02f/s", msPerFrame, fps);
            metricsCounter = GetPerformanceCounter();
        }

        lastCounter = endCounter;
    }

    return EXIT_SUCCESS;
}

static void AtExit(void)
{
    SoundDestroy();

    VideoDestroy();

    SDL_Quit();
}

#define IFOPTION(name) if (strcmp((name), argv[i]) == 0)
#define REQUIREARG()                                                           \
    if (++i >= argc) {                                                         \
        hasError = true;                                                       \
        fprintf(stderr, "Option '%s' requires an argument!\n", option);        \
        break;                                                                 \
    }
#define PARSE_UINT_ARG(min, max)                                               \
    REQUIREARG()                                                               \
    int val = atoi(argv[i]);                                                   \
    if (val >= (min) && val <= (max)) {                                        \
        arg = val;                                                             \
    } else {                                                                   \
        hasError = true;                                                       \
        fprintf(                                                               \
            stderr,                                                            \
            "Option '%s' must have int argument in range >= %d && <= %d!\n",   \
            option, (min), (max));                                             \
    }
#define PARSE_BOOL_ARG()                                                            \
    REQUIREARG()                                                                    \
    if (strcmp(argv[i], "true") == 0) {                                             \
        arg = true;                                                                 \
    } else if (strcmp(argv[i], "false") == 0) {                                     \
        arg = false;                                                                \
    } else {                                                                        \
        hasError = true;                                                            \
        fprintf(                                                                    \
            stderr,                                                                 \
            "Option '%s' argument must be a bool argument of 'true' or 'false'!\n", \
            option);                                                                \
    }

static void ParseCommandArgs(int argc, char *argv[])
{
    // No command line args provided.
    // First argument is always the program name.
    if (argc == 1) {
        return;
    }

    bool hasError = false;

    for (int i = 1; i < argc; i++) {
        const char *option = argv[i];

        IFOPTION("--win_scale")
        {
            int arg;
            PARSE_UINT_ARG(1, 8)

            if (!hasError) {
                optionWinScale = arg;
            }
        }
        else IFOPTION("--fullscreen")
        {
            bool arg;
            PARSE_BOOL_ARG()

            if (!hasError) {
                optionFullscreen = arg;
            }
        }
        else IFOPTION("--rom")
        {
            REQUIREARG()
            optionRomPath = argv[i];
        }
        else IFOPTION("--cycles")
        {
            int arg;
            PARSE_UINT_ARG(1, 1000)

            if (!hasError) {
                optionCycles = arg;
            }
        }
        else IFOPTION("--palette")
        {
            REQUIREARG()
            IFOPTION("original")
            {
                optionPalette = VMCOLOR_PALETTE_ORIGINAL;
            }
            else IFOPTION("nokia")
            {
                optionPalette = VMCOLOR_PALETTE_NOKIA;
            }
            else IFOPTION("lcd")
            {
                optionPalette = VMCOLOR_PALETTE_LCD;
            }
            else IFOPTION("hotdog")
            {
                optionPalette = VMCOLOR_PALETTE_HOTDOG;
            }
            else IFOPTION("gray")
            {
                optionPalette = VMCOLOR_PALETTE_GRAY;
            }
            else IFOPTION("cga0")
            {
                optionPalette = VMCOLOR_PALETTE_CGA0;
            }
            else IFOPTION("cga1")
            {
                optionPalette = VMCOLOR_PALETTE_CGA1;
            }
            else IFOPTION("borland")
            {
                optionPalette = VMCOLOR_PALETTE_BORLAND;
            }
            else IFOPTION("octo")
            {
                optionPalette = VMCOLOR_PALETTE_OCTO;
            }
            else
            {
                hasError = true;
                fprintf(stderr,
                        "--palette option has an unknown value of %s!\n",
                        argv[i]);
            }

            if (!hasError) {
                selectedPaletteName = argv[i];
            }
        }
        else IFOPTION("--help")
        {
            printf(
                "CHIP-8 usage:\n"
                "--rom [filepath:string]        - Load the ROM at 'filepath'. Defaults to opcode_test.ch8\n"
                "--win_scale [scale:uint]       - Scale up the screen in windowed mode. Defaults to 8\n"
                "--fullscreen [fullscreen:bool] - Start in fullscreen mode. Defaults to false\n"
                "--cycles [cycles:uint]         - Cycles to run per frame given 60fps. Defaults to 20\n"
                "                                 Value is adjusted for real framerate automatically\n"
                "--palette [palette:string]     - Use the selected color palette. Defaults to 'nokia'\n"
                "                                 Palettes: 'nokia','original','crt','hotdog','cga0','cga1',\n"
                "                                           'borland','octo'\n"
                "--help                         - Print this help text and exit\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            hasError = true;
            fprintf(stderr, "Option '%s' not recognised\n!", option);
        }
    }

    if (hasError) {
        fprintf(stderr, "Error parsing command options\n!");
    }
}
