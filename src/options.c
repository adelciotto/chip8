#include "def.h"
#include "adc_argp.h"
#include "options.h"

#define OPTIONS_SET_DEFAULTS(options)                                          \
    {                                                                          \
        (options)->windowScale = 8;                                            \
        (options)->fullscreen = false;                                         \
        (options)->romPath = "test_opcode.ch8";                                \
        (options)->cyclesPerTick = 20;                                         \
        (options)->paletteName = "nokia";                                      \
        (options)->palette = VMCOLOR_PALETTE_NOKIA;                            \
    }

static bool OptionsSetPaletteFromString(Options *options, const char *str);

void OptionsCreateFromArgv(Options *options, int argc, char *argv[])
{
    assert(options != NULL);
    OPTIONS_SET_DEFAULTS(options);

    const char *paletteName = options->paletteName;

    adc_argp_option opts[] = {
        ADC_ARGP_HELP(),
        ADC_ARGP_OPTION("fullscreen", "f", ADC_ARGP_TYPE_FLAG,
                        &options->fullscreen,
                        "Enable fullscreen mode. Defaults to off"),
        ADC_ARGP_OPTION("rom", "r", ADC_ARGP_TYPE_STRING, &options->romPath,
                        "Set the rom. Defaults to 'test_opcode.ch8'"),
        ADC_ARGP_OPTION("winscale", "w", ADC_ARGP_TYPE_UINT,
                        &options->windowScale,
                        "Set the window scale factor. Defaults to 8"),
        ADC_ARGP_OPTION(
            "cycles", "c", ADC_ARGP_TYPE_UINT, &options->cyclesPerTick,
            "Cycles to run per tick given 60 ticks per second. Defaults to 20"),
        ADC_ARGP_OPTION(
            "palette", "p", ADC_ARGP_TYPE_STRING, &paletteName,
            "Set the color palette. Defaults to 'nokia'. "
            "Palettes: 'nokia','original','lcd','crt','borland','octo','gray','hotdog','cga0','cga1'")
    };

    adc_argp_parser *parser = adc_argp_new_parser(opts, ADC_ARGP_COUNT(opts));
    if (!parser) {
        fprintf(stderr, "Failed to create arg parser\n");
        return;
    }
    if (adc_argp_parse(parser, argc, (const char **)argv) > 0)
        adc_argp_print_errors(parser, stderr);

    if (!OptionsSetPaletteFromString(options, paletteName))
        fprintf(stderr,
                "Option '--palette' option has an unknown value of %s\n",
                paletteName);
    options->windowScale = MAX(1, options->windowScale);
    options->windowScale = MIN(16, options->windowScale);
}

static bool OptionsSetPaletteFromString(Options *options, const char *str)
{
#define STR_EQL(a, b) (strcmp(a, b) == 0)

    if (STR_EQL("original", str)) {
        options->palette = VMCOLOR_PALETTE_ORIGINAL;
    } else if (STR_EQL("nokia", str)) {
        options->palette = VMCOLOR_PALETTE_NOKIA;
    } else if (STR_EQL("lcd", str)) {
        options->palette = VMCOLOR_PALETTE_LCD;
    } else if (STR_EQL("hotdog", str)) {
        options->palette = VMCOLOR_PALETTE_HOTDOG;
    } else if (STR_EQL("gray", str)) {
        options->palette = VMCOLOR_PALETTE_GRAY;
    } else if (STR_EQL("cga0", str)) {
        options->palette = VMCOLOR_PALETTE_CGA0;
    } else if (STR_EQL("cga1", str)) {
        options->palette = VMCOLOR_PALETTE_CGA1;
    } else if (STR_EQL("borland", str)) {
        options->palette = VMCOLOR_PALETTE_BORLAND;
    } else if (STR_EQL("octo", str)) {
        options->palette = VMCOLOR_PALETTE_OCTO;
    } else {
        return false;
    }

    options->paletteName = str;
    return true;

#undef STR_EQL
}
