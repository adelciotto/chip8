#ifndef CHIP8_OPTIONS_H
#define CHIP8_OPTIONS_H

#include "vm.h"

typedef struct tOptions {
    int windowScale;
    bool fullscreen;
    const char *romPath;
    int cyclesPerTick;
    const char *paletteName;
    VMColorPaletteType palette;
} Options;

void OptionsCreateFromArgv(Options *options, int argc, char *argv[]);

#endif //CHIP8_OPTIONS_H
