#include "vm.h"

struct VM {
    Chip8 chip8;
    VMColorPalette palette;

    int cyclesPerTic;
    double timerDelta;
    double timerAccum;

    bool paused;
    bool initialized;
};

static struct VM vm;

// clang-format off
static VMColorPalette palettes[] = {
	{ 0xFF000000, 0xFFFFFFFF },		// PALETTE_ORIGINAL
	{ 0xFF43523D, 0xFFC7F0D8 },		// PALETTE_NOKIA
	{ 0xFFF9FFB3, 0xFF3D8026 },	    // PALETTE_LCD
	{ 0xFF000000, 0xFFFF0000 },     // PALETTE_HOTDOG
	{ 0xFFAAAAAA, 0xFF000000 },     // PALETTE_GRAY
	{ 0xFF000000, 0xFF00FF00 },     // PALETTE_CGA0
	{ 0xFF000000, 0xFFFF00FF },     // PALETTE_CGA1
	{ 0xFF0000FF, 0xFFFFFF00 },     // PALETTE_BORLAND
	{ 0xFFAA4400, 0xFFFFAA00 },	    // PALETTE_OCTO
};
// clang-format on

void VMInit(int cyclesPerTic, VMColorPaletteType paletteType)
{
    Chip8Init(&vm.chip8);
    memcpy(vm.palette, palettes[paletteType], sizeof(VMColorPalette));

    vm.cyclesPerTic = cyclesPerTic;
    vm.timerDelta = 1.0 / (double)CHIP8_TIMER_FREQ;
    vm.timerAccum = 0;

    vm.paused = false;
    vm.initialized = true;
}

int VMLoadRom(const char *filePath)
{
    assert(vm.initialized);
    assert(filePath != NULL);

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
    size_t bytesRead =
        fread(vm.chip8.memory + CHIP8_USERMEM_START, 1, sz, file);
    if (bytesRead != sz) {
        fprintf(
            stderr,
            "Rom %s file read not complete! Read %zu bytes, total is %zu bytes\n",
            filePath, bytesRead, sz);
        goto error;
    }

    // Set the CHIP-8 program counter to the start of user memory.
    vm.chip8.PC = CHIP8_USERMEM_START;

    fclose(file);
    return 0;

error:
    if (file) {
        fclose(file);
    }
    return -1;
}

void VMTic(double deltaPerTic)
{
    assert(vm.initialized);

    if (vm.paused) {
        return;
    }

    // Execute CHIP8 instructions at correct rate.
    for (int c = 0; (c < vm.cyclesPerTic) && !Chip8WaitingForKey(&vm.chip8);
         c++) {
        Chip8Cycle(&vm.chip8);
    }

    // Update the timers at 60hz.
    vm.timerAccum += deltaPerTic;
    while (vm.timerAccum >= vm.timerDelta) {
        if (vm.chip8.delayTimer > 0) {
            vm.chip8.delayTimer--;
        }
        if (vm.chip8.soundTimer > 0) {
            vm.chip8.soundTimer--;
        }

        vm.timerAccum -= deltaPerTic;
    }
}

uint8_t *VMGetDisplayPixels()
{
    assert(vm.initialized);

    return vm.chip8.display;
}

int VMGetSoundTimer()
{
    assert(vm.initialized);

    return (int)vm.chip8.soundTimer;
}

void VMGetColorPalette(VMColorPalette outPalette)
{
    assert(vm.initialized);

    memcpy(outPalette, vm.palette, sizeof(VMColorPalette));
}

void VMSetKey(uint8_t key)
{
    assert(vm.initialized);

    vm.chip8.keys[key % 16] = 1;
}

void VMClearKey(uint8_t key)
{
    assert(vm.initialized);

    vm.chip8.keys[key % 16] = 0;

    if (Chip8WaitingForKey(&vm.chip8)) {
        vm.chip8.V[vm.chip8.waitingKey.reg] = key;
        vm.chip8.waitingKey.waiting = 0;
    }
}

void VMTogglePause(bool pause)
{
    assert(vm.initialized);

    vm.paused = pause;
}
