#ifndef CHIP8_VM_H
#define CHIP8_VM_H

#include <stdbool.h>
#include <stdint.h>

#include "chip8.h"

typedef enum {
    VMCOLOR_PALETTE_ORIGINAL,
    VMCOLOR_PALETTE_NOKIA,
    VMCOLOR_PALETTE_LCD,
    VMCOLOR_PALETTE_HOTDOG,
    VMCOLOR_PALETTE_GRAY,
    VMCOLOR_PALETTE_CGA0,
    VMCOLOR_PALETTE_CGA1,
    VMCOLOR_PALETTE_BORLAND,
    VMCOLOR_PALETTE_OCTO,
    VMCOLOR_PALETTE_MAX
} VMColorPaletteType;

typedef uint32_t VMColorPalette[2];

// VMInit() - Initialises the CHIP-8 VM.
void VMInit(int cycles, int refreshRate, VMColorPaletteType paletteType);

// VMLoadRom() - Loads a ROM from the given filepath into the CHIP8 system.
// Returns 0 on success and -1 on failure.
int VMLoadRom(const char *filePath);

// VMUpdate() - Updates the CHIP-8 CPU and timers.
void VMUpdate(double dt);

// VMGetDisplayPixels() - Returns a pointer to the display memory from the CHIP-8.
uint8_t *VMGetDisplayPixels();

// VMGetSoundTimer() - Returns the CHIP-8 sound timer.
int VMGetSoundTimer();

// VMGetColorPalette() - Returns the color palette to use when presenting the CHIP-8 display.
void VMGetColorPalette(VMColorPalette palette);

// VMSetKey() - Sets the key state to pressed and the waiting key register (if required).
void VMSetKey(uint8_t key);

// VMClearKey() - Sets the key state to released.
void VMClearKey(uint8_t key);

// VMTogglePause() - Toggles the pause state of the VM.
void VMTogglePause(bool pause);

#endif // CHIP8_VM_H
