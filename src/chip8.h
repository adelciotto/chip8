#ifndef _CHIP8_H_
#define _CHIP8_H_

// CHIP8 module.
// Provides low level emulation of the CHIP8 cpu, graphics and audio. Used only
// by the VM module.

#include "def.h"

#define CHIP8_W 64
#define CHIP8_H 32
#define CHIP8_USERMEM_START 0x200
#define CHIP8_USERMEM_END 0xFFF
#define CHIP8_USERMEM_TOTAL (CHIP8_USERMEM_END - CHIP8_USERMEM_START)
#define CHIP8_STACK_MAX 16

typedef union tChip8WaitingKey {
    uint8_t val;

    struct {
        // First 4 bits for the register that stores waiting key.
        unsigned int reg : 4;
        // 5th bit determines if waiting is enabled.
        unsigned int waiting : 1;
        // Unused bits.
        unsigned int unused : 3;
    };
} Chip8WaitingKey;

// The CHIP-8 system.
typedef union tChip8 {
    uint8_t memory[0x1000];

    struct {
        uint8_t V[16];
        uint8_t delayTimer;
        uint8_t soundTimer;
        uint8_t SP;
        uint8_t keys[16];
        Chip8WaitingKey waitingKey;

        uint8_t display[(CHIP8_W * CHIP8_H) / 8];
        uint8_t font[16 * 5];

        uint16_t PC;
        uint16_t stack[CHIP8_STACK_MAX];
        uint16_t I;
    };
} Chip8;

// Chip8Init() - Initialises the CHIP-8 CPU.
void Chip8Init(Chip8 *chip8);

// Chip8Cycle() - Read and execute an instruction.
void Chip8Cycle(Chip8 *chip8);

// Chip8WaitingForKey() - Returns if the CHIP8 is waiting for a key.
bool Chip8WaitingForKey(Chip8 *chip8);

#endif // _CHIP8_H_
