#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

#define DEFAULT_REFRESHRATE 60

static Chip8 chip8;
static int instructionsPerFrame;
static double timerDelta;
static double timerAccum = 0;
static bool paused;
static VMColorPalette palette;

// clang-format off
static VMColorPalette palettes[] = {
	{ 0xFF000000, 0xFFFFFFFF },		// PALETTE_ORIGINAL
	{ 0xFF43523D, 0xFFC7F0D8 },		// PALETTE_NOKIA
	{ 0xFFF9FFB3, 0xFF3D8026 },	    // PALETTE_LCD
	{ 0xFF000000, 0xFFFF0000 },     // PALETTE_HOTDOG
	{ 0xFFAAAAAA, 0xFF000000 },     // PALETTE_GRAY
	{ 0xFF000000, 0xFF00FF00 },     // PALETTE_CGA0
	{ 0xFF000000, 0xFFFF00FF },     // PALETTE_CGA1

};
// clang-format on

void VMInit(int cycles, int refreshRate, VMColorPaletteType paletteType)
{
	Chip8Init(&chip8);

	int targetFreq = cycles * DEFAULT_REFRESHRATE;
	instructionsPerFrame = MAX(targetFreq / refreshRate, 1);
	printf("VM instruction freq: %d, ins/f: %d, real refresh rate: %dhz\n",
		   targetFreq, instructionsPerFrame, refreshRate);

	timerDelta = 1.0 / (double)CHIP8_TIMER_FREQ;
	memcpy(palette, palettes[paletteType], sizeof(VMColorPalette));
	paused = false;
}

int VMLoadRom(const char *filePath)
{
	FILE *file = NULL;

	file = fopen(filePath, "rb");
	if (!file) {
		fprintf(stderr, "Failed to fopen() rom file at %s!\n", filePath);
		goto error;
	}

	// Ensure the rom program can fit into the CHIP8 user memory space.
	fseek(file, 0L, SEEK_END);
	size_t sz = ftell(file);
	if (sz > CHIP8_USERMEM_TOTAL) {
		fprintf(
			stderr,
			"Rom %s file size is too large for CHIP8! Got %zu, must be < %d\n",
			filePath, sz, CHIP8_USERMEM_TOTAL);
		goto error;
	}
	rewind(file);

	// Read the rom program into CHIP8 user memory space.
	size_t bytesRead = fread(chip8.memory + CHIP8_USERMEM_START, 1, sz, file);
	if (bytesRead != sz) {
		fprintf(
			stderr,
			"Rom %s file read not complete! Read %zu bytes, total is %zu bytes\n",
			filePath, bytesRead, sz);
		goto error;
	}

	// Set the CHIP-8 program counter to the start of user memory.
	chip8.PC = CHIP8_USERMEM_START;

	fclose(file);
	return 0;

error:
	if (file) {
		fclose(file);
	}
	return -1;
}

void VMUpdate(double dt)
{
	if (paused) {
		return;
	}

	// Update the timers at 60hz.
	timerAccum += dt;
	while (timerAccum >= timerDelta) {
		if (chip8.delayTimer > 0) {
			chip8.delayTimer--;
		}
		if (chip8.soundTimer > 0) {
			chip8.soundTimer--;
		}

		timerAccum -= dt;
	}

	// Execute CHIP8 instructions at correct rate.
	int cycles = instructionsPerFrame;
	while (cycles > 0 && !Chip8WaitingForKey(&chip8)) {
		Chip8Cycle(&chip8);
		cycles--;
	}
}

uint8_t *VMGetDisplayPixels()
{
	return chip8.display;
}

int VMGetSoundTimer()
{
	return (int)chip8.soundTimer;
}

void VMGetColorPalette(VMColorPalette outPalette)
{
	memcpy(outPalette, palette, sizeof(VMColorPalette));
}

void VMSetKey(uint8_t key)
{
	chip8.keys[key % 16] = 1;

	if (Chip8WaitingForKey(&chip8)) {
		uint8_t reg = chip8.waitingKey & 0x0F;
		chip8.V[reg] = key;
		chip8.waitingKey = 0;
	}
}

void VMClearKey(uint8_t key)
{
	chip8.keys[key % 16] = 0;
}

void VMTogglePause(bool pause)
{
	paused = pause;
}
