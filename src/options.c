#include "def.h"
#include "options.h"

#define OPTIONS_SET_DEFAULTS(options)                                          \
    {                                                                          \
        (options)->windowScale = 8;                                            \
        (options)->fullscreen = false;                                         \
        (options)->romPath = "test_opcode.ch8";                                \
        (options)->cyclesPerTick = 20;                                         \
        (options)->palette = VMCOLOR_PALETTE_NOKIA;                            \
        (options)->selectedPaletteName = "nokia";                              \
    }

static void PrintHelp();
static bool VMColorPaletteTypeFromString(VMColorPaletteType *palette,
                                         const char *str);

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

void OptionsCreateFromArgv(Options *options, int argc, char *argv[])
{
    assert(options != NULL);
    OPTIONS_SET_DEFAULTS(options);

    // No command line args provided.
    // First argument is always the program name.
    if (argc == 1) {
        return;
    }

    bool hasError = false;

    for (int i = 1; i < argc; i++) {
        const char *option = argv[i];

        IFOPTION("--window_scale")
        {
            int arg;
            PARSE_UINT_ARG(1, 8)

            if (!hasError) {
                options->windowScale = arg;
            }
        }
        else IFOPTION("--fullscreen")
        {
            bool arg;
            PARSE_BOOL_ARG()

            if (!hasError) {
                options->fullscreen = arg;
            }
        }
        else IFOPTION("--rom")
        {
            REQUIREARG()
            options->romPath = argv[i];
        }
        else IFOPTION("--cycles")
        {
            int arg;
            PARSE_UINT_ARG(1, 1000)

            if (!hasError) {
                options->cyclesPerTick = arg;
            }
        }
        else IFOPTION("--palette")
        {
            REQUIREARG()

            if (!VMColorPaletteTypeFromString(&options->palette, argv[i])) {
                hasError = true;
                fprintf(stderr,
                        "--palette option has an unknown value of %s!\n",
                        argv[i]);
            }

            if (!hasError) {
                options->selectedPaletteName = argv[i];
            }
        }
        else IFOPTION("--help")
        {
            PrintHelp();
            exit(EXIT_SUCCESS);
        }
        else
        {
            hasError = true;
            fprintf(stderr, "Option '%s' not recognised!\n", option);
        }
    }

    if (hasError) {
        fprintf(stderr, "Error parsing command options!\n");
        PrintHelp();
    }
}

static void PrintHelp()
{
    printf(
        "CHIP-8 usage:\n"
        "--rom [filepath:string]        - Load the ROM at 'filepath'. Defaults to opcode_test.ch8\n"
        "--window_scale [scale:uint]    - Scale up the screen in windowed mode. Defaults to 8\n"
        "--fullscreen [fullscreen:bool] - Start in fullscreen mode. Defaults to false\n"
        "--cycles [cycles:uint]         - Cycles to run per tick given 60 ticks per second. Defaults to 20\n"
        "                                 Value is adjusted for real framerate automatically\n"
        "--palette [palette:string]     - Use the selected color palette. Defaults to 'nokia'\n"
        "                                 Palettes: 'nokia','original','crt','hotdog','cga0','cga1',\n"
        "                                           'borland','octo'\n"
        "--help                         - Print this help text and exit\n");
}

static bool VMColorPaletteTypeFromString(VMColorPaletteType *palette,
                                         const char *str)
{
#define STR_EQL(a, b) (strcmp(a, b) == 0)

    if (STR_EQL("original", str)) {
        *palette = VMCOLOR_PALETTE_ORIGINAL;
    } else if (STR_EQL("nokia", str)) {
        *palette = VMCOLOR_PALETTE_NOKIA;
    } else if (STR_EQL("lcd", str)) {
        *palette = VMCOLOR_PALETTE_LCD;
    } else if (STR_EQL("hotdog", str)) {
        *palette = VMCOLOR_PALETTE_HOTDOG;
    } else if (STR_EQL("gray", str)) {
        *palette = VMCOLOR_PALETTE_GRAY;
    } else if (STR_EQL("cga0", str)) {
        *palette = VMCOLOR_PALETTE_CGA0;
    } else if (STR_EQL("cga1", str)) {
        *palette = VMCOLOR_PALETTE_CGA1;
    } else if (STR_EQL("borland", str)) {
        *palette = VMCOLOR_PALETTE_BORLAND;
    } else if (STR_EQL("octo", str)) {
        *palette = VMCOLOR_PALETTE_OCTO;
    } else {
        return false;
    }

    return true;

#undef STR_EQL
}
