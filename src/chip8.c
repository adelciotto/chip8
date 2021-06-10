#include "chip8.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>

struct UNNN {
    unsigned int nnn : 12;
    unsigned int u : 4;
};
struct UXKK {
    unsigned int kk : 8;
    unsigned int x : 4;
    unsigned int u : 4;
};
struct UXYN {
    unsigned int n : 4;
    unsigned int y : 4;
    unsigned int x : 4;
    unsigned int u : 4;
};

typedef union tOpcode {
    uint16_t val;

    struct UNNN unnn;
    struct UXKK uxkk;
    struct UXYN uxyn;
} Opcode;

static uint8_t fontData[16 * 5] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // '0'
    0x20, 0x60, 0x20, 0x20, 0x70, // '1'
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // '2'
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // '3'
    0x90, 0x90, 0xF0, 0x10, 0x10, // '4'
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // '5'
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // '6'
    0xF0, 0x10, 0x20, 0x40, 0x40, // '7'
    0xF0, 0x90, 0xF0, 0x90, 0x90, // '8'
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // '9'
    0xF0, 0x90, 0xF0, 0x90, 0x90, // 'A'
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // 'B'
    0xF0, 0x80, 0x80, 0x80, 0xF0, // 'C'
    0xE0, 0x90, 0x90, 0x90, 0xE0, // 'D'
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // 'E'
    0xF0, 0x80, 0xF0, 0x80, 0x80, // 'F'
};

static Opcode FetchOpcode(Chip8 *chip8);
static void DecodeAndExecOpcode(Chip8 *chip8, Opcode op);

void Chip8Init(Chip8 *chip8)
{
    // Seed the rng.
    srand(time(NULL));

    // Reset all of the CHIP8 memory.
    memset(chip8->memory, 0, 0x1000);

    // Copy over the font data.
    memcpy(chip8->font, fontData, 16 * 5);
}

void Chip8Cycle(Chip8 *chip8)
{
    // Fetch the bytes and increment the program counter.
    Opcode ins = FetchOpcode(chip8);

    // Decode and execute the instruction.
    DecodeAndExecOpcode(chip8, ins);
}

bool Chip8WaitingForKey(Chip8 *chip8)
{
    return (chip8->waitingKey.waiting == 1) ? true : false;
}

static Opcode FetchOpcode(Chip8 *chip8)
{
    uint8_t upper = chip8->memory[chip8->PC];
    uint8_t lower = chip8->memory[chip8->PC + 1];
    uint16_t value = (upper << 8) | lower;

    chip8->PC += 2;

    return (Opcode){ .val = value };
}

static void DecodeAndExecOpcode(Chip8 *chip8, Opcode op)
{
    struct UNNN unnn = op.unnn;
    struct UXKK uxkk = op.uxkk;
    struct UXYN uxyn = op.uxyn;
    uint8_t u = unnn.u;

    uint8_t *mem = chip8->memory;
    uint8_t *display = chip8->display;
    uint8_t *V = chip8->V;
    uint16_t *stack = chip8->stack;

    if (u == 0) {
        // 00E0 CLS - Clear the display.
        if (op.val == 0x00E0) {
            for (int i = 0; i < ARRAY_LEN(chip8->display); i++)
                display[i] = 0;
        }
        // 0x00EE RET - Return from a subroutine.
        if (op.val == 0x00EE) {
            // Set PC to address at top of stack, then decrement SP.
            chip8->PC = stack[chip8->SP-- % CHIP8_STACK_MAX];
        }
    }
    // 1nnn JP, addr - Sets the PC to nnn.
    if (u == 1) {
        chip8->PC = unnn.nnn;
    }
    // 2nnn CALL, addr - Calls the subroutine at nnn.
    if (u == 2) {
        stack[++chip8->SP % CHIP8_STACK_MAX] =
            chip8->PC; // Increment SP, then push current PC on top.
        chip8->PC = unnn.nnn;
    }
    // 3xkk SE Vx, byte - Skip next instruction if Vx == kk.
    if (u == 3) {
        if (V[uxkk.x] == (uint8_t)uxkk.kk) {
            chip8->PC += 2;
        }
    }
    // 4xkk SNE Vx, byte - Skip next instruction if Vx != kk.
    if (u == 4) {
        if (V[uxkk.x] != (uint8_t)uxkk.kk) {
            chip8->PC += 2;
        }
    }
    // 5xy0 SE Vx, Vy - Skip next instruction if Vx == Vy.
    if (u == 5) {
        if (V[uxyn.x] == V[uxyn.y]) {
            chip8->PC += 2;
        }
    }
    // 6xkk LD Vx, byte - Puts the value kk into Vx.
    if (u == 6) {
        V[uxkk.x] = uxkk.kk;
    }
    // 7xkk ADD Vx, byte - Adds the value kk to Vx, then stores result in Vx.
    if (u == 7) {
        V[uxkk.x] += (uint8_t)uxkk.kk;
    }
    // 8xy0 LD Vx, Vy - Puts the value in Vy into Vx.
    if (u == 8 && (uint8_t)uxyn.n == 0) {
        V[uxyn.x] = V[uxyn.y];
    }
    // 8xy1 OR Vx, Vy - Performs bitwise OR of Vx and Vy, then stores result in Vx.
    if (u == 8 && (uint8_t)uxyn.n == 1) {
        V[uxyn.x] |= V[uxyn.y];
    }
    // 8xy2 AND Vx, Vy - Performs bitwise AND of Vx and Vy, then stores result in Vx.
    if (u == 8 && (uint8_t)uxyn.n == 2) {
        V[uxyn.x] &= V[uxyn.y];
    }
    // 8xy2 XOR Vx, Vy - Performs bitwise AND of Vx and Vy, then stores result in Vx.
    if (u == 8 && (uint8_t)uxyn.n == 3) {
        V[uxyn.x] ^= V[uxyn.y];
    }
    // 8xy4 ADD Vx, Vy - Adds Vy to Vx. Stores carry flag in VF.
    if (u == 8 && (uint8_t)uxyn.n == 4) {
        int result = V[uxyn.x] += V[uxyn.y];
        V[0xF] = (result > 255);
    }
    // 8xy5 SUB Vx, Vy - Subtracts Vy from Vx. Stores borrow flag in VF.
    if (u == 8 && (uint8_t)uxyn.n == 5) {
        V[0xF] = (V[uxyn.x] > V[uxyn.y]);
        V[uxyn.x] -= V[uxyn.y];
    }
    // 8xy6 SHR Vx - Stores Vx lsb in VF, then shifts Vx to the right by 1.
    if (u == 8 && (uint8_t)uxyn.n == 6) {
        V[0xF] = (V[uxyn.x] & (1 << 0));
        V[uxyn.x] >>= 1;
    }
    // 8xy7 SUBN Vx, Vy - Subtracts Vx from Vy. Stores borrow flag in VF.
    if (u == 8 && (uint8_t)uxyn.n == 7) {
        V[0xF] = (V[uxyn.y] > V[uxyn.x]);
        V[uxyn.x] = V[uxyn.y] - V[uxyn.x];
    }
    // 8xyE SHL Vx - Stores Vx msb in VF, then shifts Vx to the left by 1.
    if (u == 8 && (uint8_t)uxyn.n == 0x0E) {
        V[0xF] = (V[uxyn.x] & (1 << 7));
        V[uxyn.x] <<= 1;
    }
    // 9xy0 SNE Vx, Vy - Skips the next instruction if Vx != Vy.
    if (u == 9 && (uint8_t)uxyn.n == 0) {
        if (V[uxyn.x] != V[uxyn.y]) {
            chip8->PC += 2;
        }
    }
    // Annn LD I, addr - Sets I to nnn.
    if (u == 0xA) {
        chip8->I = unnn.nnn;
    }
    // Bnnn JP V0, addr - Sets PC to nnn plus the value of V0.
    if (u == 0xB) {
        chip8->PC = (uint16_t)unnn.nnn + V[0];
    }
    // Cxkk RND Vx, byte - Stores random number (between 0 and 255) ANDed with kk in Vx.
    if (u == 0xC) {
        V[uxkk.x] = (rand() % 256) & (uint8_t)uxkk.kk;
    }
    // Dxyn DRW Vx, Vy, nibble - Display n-byte sprite starting at mem location I at (Vx, Vy).
    // Set VF to 1 if collision.
    if (u == 0xD) {
        // Clear the collision register.
        V[0xF] = 0;
        // Calculate the start and end draw coordinates.
        int startX = V[uxyn.x] % CHIP8_W;
        int startY = V[uxyn.y] % CHIP8_H;
        int endX = MIN(startX + 8, CHIP8_W);
        int endY = MIN(startY + uxyn.n, CHIP8_H);

        // Loop over each row of the sprite.
        for (int yline = startY; yline < endY; yline++) {
            // Get the current byte from sprite.
            uint8_t spriteB = mem[chip8->I + (yline - startY)];
            // Loop over each pixel of the sprite and determine if draw needed.
            for (int xline = startX; xline < endX; xline++) {
                // Get the pixel in sprite byte.
                uint8_t spriteP =
                    (spriteB & (0x80 >> (xline - startX))) ? 1 : 0;

                // Skip if the sprite pixel is not set.
                if (!spriteP)
                    continue;

                // Get the display byte and pixel.
                int index = yline * CHIP8_W + xline;
                int dispIndx = index / 8;
                int offset = index % 8;
                uint8_t dispB = display[dispIndx];
                uint8_t dispP = (dispB & (0x80 >> offset)) ? 1 : 0;

                // If the current display pixel is not on, then set it.
                if (!dispP) {
                    display[dispIndx] |= (0x80 >> offset);
                }
                // If it is on, then clear it and set the collision flag.
                else {
                    display[dispIndx] &= (~(0x80 >> offset));
                    V[0xF] = 1;
                }
            }
        }
    }
    // Ex9E SKP Vx - Skips next instruction if key with value of Vx is pressed.
    if (u == 0xE && (uint8_t)uxkk.kk == 0x9E) {
        if (chip8->keys[V[uxkk.x]]) {
            chip8->PC += 2;
        }
    }
    // ExA1 SKNP Vx - Skips next instruction if key with value of Vx is not pressed.
    if (u == 0xE && (uint8_t)uxkk.kk == 0xA1) {
        if (!chip8->keys[V[uxkk.x]]) {
            chip8->PC += 2;
        }
    }
    // Fx07 LD Vx, DT - Loads value of delay timer into Vx.
    if (u == 0xF && (uint8_t)uxkk.kk == 0x07) {
        V[uxkk.x] = chip8->delayTimer;
    }
    // Fx0A LD Vx, K - Wait for key press, then the value of the key is stored in Vx.
    if (u == 0xF && (uint8_t)uxkk.kk == 0x0A) {
        chip8->waitingKey.waiting = 1;
        chip8->waitingKey.reg = uxkk.x;
    }
    // Fx15 LD DT, Vx - Set the delay timer to Vx.
    if (u == 0xF && (uint8_t)uxkk.kk == 0x15) {
        chip8->delayTimer = V[uxkk.x];
    }
    // Fx18 LD ST, Vx - Set the sound timer to Vx.
    if (u == 0xF && (uint8_t)uxkk.kk == 0x18) {
        chip8->soundTimer = V[uxkk.x];
    }
    // Fx1E ADD I, Vx - Add I and Vx, then store the result in I.
    if (u == 0xF && (uint8_t)uxkk.kk == 0x1E) {
        chip8->I += V[uxkk.x];
    }
    // Fx29 LD F, Vx - Set I to location of sprite for digit Vx.
    if (u == 0xF && (uint8_t)uxkk.kk == 0x29) {
        chip8->I = &chip8->font[(V[uxkk.x] & 15) * 5] - mem;
    }
    // Fx33 LD B, Vx - Store BCD representation of Vx in I, I+1, I+2.
    if (u == 0xF && (uint8_t)uxkk.kk == 0x33) {
        uint8_t x = uxkk.x;
        mem[chip8->I] = V[x] / 100;
        mem[chip8->I + 1] = (V[x] / 10) % 10;
        mem[chip8->I + 2] = (V[x] % 10);
    }
    // Fx55 LD [I], Vx - Store registers V0 to Vx in memory locations starting at I.
    if (u == 0xF && (uint8_t)uxkk.kk == 0x55) {
        uint8_t x = uxkk.x;
        for (uint8_t i = 0; i <= x; i++) {
            mem[chip8->I + i] = V[i];
        }
    }
    // Fx65 LD [I], Vx - Read registers V0 to Vx from memory locations starting at I.
    if (u == 0xF && (uint8_t)uxkk.kk == 0x65) {
        uint8_t x = uxkk.x;
        for (uint8_t i = 0; i <= x; i++) {
            V[i] = mem[chip8->I + i];
        }
    }
}
