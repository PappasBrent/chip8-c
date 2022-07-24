// see http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
// see https://lazyfoo.net/tutorials/SDL/01_hello_SDL/index2.php

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include <iostream>
#include <chrono>
#include <thread>

#include "SDL2/SDL.h"

const constexpr size_t MEM_NBYTES = 4096;
const constexpr size_t INTERP_NBYTES = 512;
const constexpr size_t PROGRAM_NBYTES = MEM_NBYTES - INTERP_NBYTES;
const constexpr uint16_t PROGRAM_START = 0x200;

class Chip8
{
private:
    uint8_t V[16]; // registers
    uint8_t DT;    // delay timer
    uint8_t ST;    // sound timer
    uint8_t SP;    // stack pointer

    uint16_t I;         // I register
    uint16_t stack[16]; // call stack

public:
    bool keys[16];              // keys
    uint8_t memory[MEM_NBYTES]; // memory
    uint16_t PC;                // program counter
    // 32 x 64 = 2048 bit screen
    // we emulate this via a bit array
    uint8_t display[32 * 64 / 8];

    // draw flag, not part of chip 8 specs but reduces flickering
    bool draw;

    Chip8() = default;

    void reset()
    {
        memset(memory, 0, MEM_NBYTES);
        // builtin sprites
        constexpr const uint8_t BUILTIN_SPRITES[] = {
            0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
            0x20, 0x60, 0x20, 0x20, 0x70, // 1
            0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
            0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
            0x90, 0x90, 0xF0, 0x10, 0x10, // 4
            0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
            0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
            0xF0, 0x10, 0x20, 0x40, 0x40, // 7
            0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
            0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
            0xF0, 0x90, 0xF0, 0x90, 0x90, // A
            0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
            0xF0, 0x80, 0x80, 0x80, 0xF0, // C
            0xE0, 0x90, 0x90, 0x90, 0xE0, // D
            0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
            0xF0, 0x80, 0xF0, 0x80, 0x80  // F
        };
        memcpy(memory, BUILTIN_SPRITES, 80);
        memset(V, 0, 16);
        DT = 0;
        ST = 0;
        SP = 0;
        for (size_t i = 0; i < 16; i++)
        {
            keys[i] = false;
        }
        PC = PROGRAM_START;
        I = 0;
        memset(stack, 0, 16 * 2);
        memset(display, 0, 32 * 64 / 8);
    }

    void loadROMFromPath(const char *path)
    {
        reset();

        // load rom
        FILE *rom = fopen(path, "rb");
        assert(rom != nullptr);

        // get rom size
        fseek(rom, 0, SEEK_END);
        long romSz = ftell(rom);
        rewind(rom);

        // TODO: Free if this assert fails
        assert(fread(memory + PROGRAM_START, (sizeof(uint8_t)), romSz, rom) == romSz);
        fclose(rom);
    }

    void executeCycle()
    {
        // instructions are two bytes long
        const uint16_t instruction = (memory[PC] << 8) | memory[PC + 1];
        const uint8_t op = (instruction & 0xF000) >> 12;
        const uint8_t x = (instruction & 0x0F00) >> 8;
        const uint8_t y = (instruction & 0x00F0) >> 4;
        const uint16_t nnn = instruction & 0x0FFF;
        const uint8_t kk = instruction & 0x00FF;
        const uint8_t n = instruction & 0x000F;

        // execute instruction
        // The original implementation of the Chip-8 language includes 36
        // different instructions, including math, graphics, and flow control
        // functions.

        if (op == 0)
        {
            // 00E0 - CLS
            if (n == 0x0)
            {
                // Clear the display.
                memset(display, 0, 32 * 64 / 8);
                draw = true;
                PC += 2;
            }
            // 00EE - RET
            else if (n == 0xE)
            {
                // Return from a subroutine.
                // The interpreter sets the program counter to the address at
                // the top of the stack, then subtracts 1 from the stack pointer.
                SP--;
                PC = stack[SP];
                PC += 2;
            }
        }
        // 1nnn - JP addr
        else if (op == 0x1)
        {
            // Jump to location nnn.
            // The interpreter sets the program counter to nnn.
            PC = nnn;
        }
        // 2nnn - CALL addr
        else if (op == 0x2)
        {
            // Call subroutine at nnn.
            // The interpreter increments the stack pointer, then puts
            // the current PC on the top of the stack.
            // The PC is then set to nnn.
            stack[SP] = PC;
            SP++;
            PC = nnn;
        }
        // 3xkk - SE Vx, byte
        else if (op == 0x3)
        {
            // Skip next instruction if Vx = kk.
            // The interpreter compares register Vx to kk, and if they are equal,
            // increments the program counter by 2.
            PC += (V[x] == kk) ? 4 : 2;
        }
        // 4xkk - SNE Vx, byte
        else if (op == 0x4)
        {
            // Skip next instruction if Vx != kk.
            // The interpreter compares register Vx to kk, and if they
            // are not equal, increments the program counter by 2.
            PC += (V[x] != kk) ? 4 : 2;
        }
        // 5xy0 - SE Vx, Vy
        else if (op == 0x5)
        {
            // Skip next instruction if Vx = Vy.
            // The interpreter compares register Vx to register Vy, and if they
            // are equal, increments the program counter by 2.
            PC += (V[x] == V[y]) ? 4 : 2;
        }
        // 6xkk - LD Vx, byte
        else if (op == 0x6)
        {
            // Set Vx = kk.
            // The interpreter puts the value kk into register Vx.
            V[x] = kk;
            PC += 2;
        }
        // 7xkk - ADD Vx, byte
        else if (op == 0x7)
        {
            // Set Vx = Vx + kk.
            // Adds the value kk to the value of register Vx,
            // then stores the result in Vx.
            V[x] += kk;
            PC += 2;
        }
        else if (op == 0x8)
        {
            // 8xy0 - LD Vx, Vy
            if (n == 0x0)
            {
                // Set Vx = Vy.
                // Stores the value of register Vy in register Vx.
                V[x] = V[y];
                PC += 2;
            }
            // 8xy1 - OR Vx, Vy
            else if (n == 0x1)
            {
                // Set Vx = Vx OR Vy.
                // Performs a bitwise OR on the values of Vx and Vy, then stores
                // the result in Vx.
                // A bitwise OR compares the corresponding bits from two values,
                // and if either bit is 1, then the same bit in the result is
                // also 1.
                // Otherwise, it is 0.
                V[x] |= V[y];
                PC += 2;
            }
            // 8xy2 - AND Vx, Vy
            else if (n == 0x2)
            {
                // Set Vx = Vx AND Vy.
                // Performs a bitwise AND on the values of Vx and Vy, then stores
                // the result in Vx.
                // A bitwise AND compares the corresponding bits from two values,
                // and if both bits are 1, then the same bit in the result is
                // also 1.
                // Otherwise, it is 0.
                V[x] &= V[y];
                PC += 2;
            }
            // 8xy3 - XOR Vx, Vy
            else if (n == 0x3)
            {
                // Set Vx = Vx XOR Vy.
                // Performs a bitwise exclusive OR on the values of Vx and Vy,
                // then stores the result in Vx.
                // An exclusive OR compares the corresponding bits from two values,
                // and if the bits are not both the same, then the corresponding
                // bit in the result is set to 1.
                // Otherwise, it is 0.
                V[x] ^= V[y];
                PC += 2;
            }
            // 8xy4 - ADD Vx, Vy
            else if (n == 0x4)
            {
                // Set Vx = Vx + Vy, set VF = carry.
                // The values of Vx and Vy are added together.
                // If the result is greater than 8 bits (i.e., > 255,) VF is
                // set to 1, otherwise 0.
                // Only the lowest 8 bits of the result are kept, and stored in Vx.

                V[0xF] = (V[x] + V[y] > 255) ? 1 : 0;
                V[x] += V[y];
                PC += 2;
            }
            // 8xy5 - SUB Vx, Vy
            else if (n == 0x5)
            {
                // Set Vx = Vx - Vy, set VF = NOT borrow.
                // If Vx > Vy, then VF is set to 1, otherwise 0.
                // Then Vy is subtracted from Vx, and the results stored in Vx.
                V[0xF] = (V[x] > V[y]) ? 1 : 0;
                V[x] -= V[y];
                PC += 2;
            }
            // 8xy6 - SHR Vx {, Vy}
            else if (n == 0x6)
            {
                // Set Vx = Vx SHR 1.
                // If the least-significant bit of Vx is 1, then VF is set to 1,
                // otherwise 0.
                // Then Vx is divided by 2.
                V[0xF] = V[x] & 1;
                V[x] >>= 1;
                PC += 2;
            }
            // 8xy7 - SUBN Vx, Vy
            else if (n == 0x7)
            {
                // Set Vx = Vy - Vx, set VF = NOT borrow.
                // If Vy > Vx, then VF is set to 1, otherwise 0.
                // Then Vx is subtracted from Vy, and the results stored in Vx.
                V[0xF] = (V[y] > V[x]) ? 1 : 0;
                V[x] = V[y] - V[x];
                PC += 2;
            }
            // 8xyE - SHL Vx {, Vy}
            else if (n == 0xE)
            {
                // Set Vx = Vx SHL 1.
                // If the most-significant bit of Vx is 1, then VF is set to 1,
                // otherwise to 0.
                // Then Vx is multiplied by 2.
                V[0xF] = (V[x] >> 7);
                V[x] <<= 1;
                PC += 2;
            }
        }
        // 9xy0 - SNE Vx, Vy
        else if (op == 0x9)
        {
            // Skip next instruction if Vx != Vy.
            // The values of Vx and Vy are compared, and if they are not equal,
            // the program counter is increased by 2.
            PC += (V[x] != V[y]) ? 4 : 2;
        }
        // Annn - LD I, addr
        else if (op == 0xA)
        {
            // Set I = nnn.
            // The value of register I is set to nnn.
            I = nnn;
            PC += 2;
        }
        // Bnnn - JP V0, addr
        else if (op == 0xB)
        {
            // Jump to location nnn + V0.
            // The program counter is set to nnn plus the value of V0.
            PC = nnn + V[0];
        }
        // Cxkk - RND Vx, byte
        else if (op == 0xC)
        {
            // Set Vx = random byte AND kk.
            // The interpreter generates a random number from 0 to 255,
            // which is then ANDed with the value kk.
            // The results are stored in Vx.
            // See instruction 8xy2 for more information on AND.
            V[x] = ((rand() % 256) & kk);
            printf("%d\n", V[x]);
            PC += 2;
        }
        // Dxyn - DRW Vx, Vy, nibble
        else if (op == 0xD)
        {
            // Display n-byte sprite starting at memory location I at (Vx, Vy),
            // set VF = collision.
            // The interpreter reads n bytes from memory,
            // starting at the address stored in I.
            // These bytes are then displayed as sprites
            // on screen at coordinates (Vx, Vy).
            // Sprites are XORed onto the existing screen.
            // If this causes any pixels to be erased, VF is set to 1,
            // otherwise it is set to 0.
            // If the sprite is positioned so part of it is outside the
            // coordinates of the display, it wraps around to the opposite
            // side of the screen.
            // See instruction 8xy3 for more information on XOR, and
            // section 2.4, Display,
            // for more information on the Chip-8 screen and sprites.

            // NOTE: look for errors here first

            // init VF to zero, then check for collision
            V[0xF] = 0;

            size_t startCol = V[x];
            size_t startRow = V[y];

            // read each byte
            for (size_t offsetRow = 0; offsetRow < n; offsetRow++)
            {
                uint8_t nthByte = memory[I + offsetRow];
                size_t pxRow = (startRow + offsetRow) % 32;

                // read each bit (pixel)
                for (size_t offsetCol = 0; offsetCol < 8; offsetCol++)
                {
                    // get the offsetCol'th bit from the left end of this byte
                    size_t byteShamt = 7 - offsetCol;
                    int bytePxVal = (nthByte & (1 << byteShamt)) >> byteShamt;

                    size_t pxCol = (startCol + offsetCol) % 64;

                    // bit array indices
                    size_t bitIndex = (pxRow * 64) + pxCol;
                    size_t byteIndex = bitIndex / 8;
                    size_t dispShamt = 7 - (bitIndex % 8);

                    // get the value of the bit in the display bit array
                    // xor the display
                    bool originalPxVal = ((display[byteIndex]) & (1 << dispShamt)) >> dispShamt;
                    bool newPxVal = originalPxVal ^ bytePxVal;

                    // set the new value
                    display[byteIndex] ^= ((bytePxVal) << dispShamt);

                    // update VF
                    V[0xF] = V[0xF] | ((originalPxVal && (!newPxVal)) ? 1 : 0);
                }
            }

            draw = true;
            PC += 2;
        }
        else if (op == 0xE)
        {
            // Ex9E - SKP Vx
            if (kk == 0x9E)
            {
                // Skip next instruction if key with the value of Vx is pressed.
                // Checks the keyboard, and if the key corresponding to
                // the value of Vx is currently in the down position,
                // PC is increased by 2.
                PC += (keys[V[x]]) ? 4 : 2;
            }
            // ExA1 - SKNP Vx
            else if (kk == 0xA1)
            {
                // Skip next instruction if key with the val of Vx is not pressed.
                // Checks the keyboard, and if the key corresponding to
                // the value of Vx is currently in the up position,
                // PC is increased by 2.
                PC += (!keys[V[x]]) ? 4 : 2;
            }
        }
        else if (op == 0xF)
        {
            // Fx07 - LD Vx, DT
            if (kk == 0x07)
            {
                // Set Vx = delay timer value.
                // The value of DT is placed into Vx.
                V[x] = DT;
                PC += 2;
            }
            // Fx0A - LD Vx, K
            else if (kk == 0x0A)
            {
                // Wait for a key press, store the value of the key in Vx.
                // All execution stops until a key is pressed, then the value of that key is stored in Vx.

                // check if a key was pressed, and if not,
                // then perform this instruction again
                bool keyPressed = false;
                for (size_t i = 0; i < 16; i++)
                {
                    keyPressed = keyPressed || keys[i];
                    V[x] = keyPressed ? i : V[x];
                }

                // don't increment PC if key not pressed
                if (!keyPressed)
                {
                    return;
                }

                PC += 2;
            }
            // Fx15 - LD DT, Vx
            else if (kk == 0x15)
            {
                // Set delay timer = Vx.
                // DT is set equal to the value of Vx.
                DT = V[x];
                PC += 2;
            }
            // Fx18 - LD ST, Vx
            else if (kk == 0x18)
            {
                // Set sound timer = Vx.
                // ST is set equal to the value of Vx.
                ST = V[x];
                PC += 2;
            }
            // Fx1E - ADD I, Vx
            else if (kk == 0x1E)
            {
                // Set I = I + Vx.
                // The values of I and Vx are added,
                // and the results are stored in I.
                // VF is set to 1 when range overflow (I+VX>0xFFF),
                // and 0 when it isn't
                V[0xF] = (I + V[x]) > 0xFFF;
                I += V[x];
                PC += 2;
            }
            else if (kk == 0x29)
            {
                // Fx29 - LD F, Vx
                // Set I = location of sprite for digit Vx.
                // The value of I is set to the location for
                // the hexadecimal sprite corresponding to the value of Vx.
                // See section 2.4, Display, for more information on
                // the Chip-8 hexadecimal font.
                I = 5 * V[x];
                PC += 2;
            }
            // Fx33 - LD B, Vx
            else if (kk == 0x33)
            {
                // Store BCD representation of Vx in memory
                // locations I, I+1, and I+2.
                // The interpreter takes the decimal value of Vx,
                // and places the hundreds digit in memory at location in I,
                // the tens digit at location I+1, and the ones digit at
                // location I+2.
                memory[I] = V[x] / 100;
                memory[I + 1] = (V[x] / 10) % 10;
                memory[I + 2] = V[x] % 10;
                PC += 2;
            }
            // Fx55 - LD [I], Vx
            else if (kk == 0x55)
            {
                // Store registers V0 through Vx in memory starting at location I.
                // The interpreter copies the values of registers V0 through Vx
                // into memory, starting at the address in I.
                memcpy(memory + I, V, x + 1);
                I += x + 1;
                PC += 2;
            }
            // Fx65 - LD Vx, [I]
            else if (kk == 0x65)
            {
                // Read registers V0 through Vx from memory starting at location I.
                // The interpreter reads values from memory starting at location I
                // into registers V0 through Vx.
                memcpy(V, memory + I, x + 1);
                I += x + 1;
                PC += 2;
            }
        }

        // update timers
        if (DT > 0)
        {
            DT--;
        }
        if (ST > 0)
        {
            ST--;
        }
    }
};

// Keypad keymap
uint8_t keymap[16] = {
    SDLK_x,
    SDLK_1,
    SDLK_2,
    SDLK_3,
    SDLK_q,
    SDLK_w,
    SDLK_e,
    SDLK_a,
    SDLK_s,
    SDLK_d,
    SDLK_z,
    SDLK_c,
    SDLK_4,
    SDLK_r,
    SDLK_f,
    SDLK_v,
};

int main(int argc, char const *argv[])
{
    Chip8 c8;

    if (argc < 2)
    {
        std::cout << "USAGE: ./main ROM\n";
        return 1;
    }

    c8.loadROMFromPath(argv[1]);

    int w = 1024; // Window width
    int h = 512;  // Window height

    // The window we'll be rendering to
    SDL_Window *window = NULL;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        exit(1);
    }
    // Create window
    window = SDL_CreateWindow(
        "CHIP-8 Emulator",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        w, h, SDL_WINDOW_SHOWN);
    if (window == NULL)
    {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        exit(2);
    }

    // Create renderer
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_RenderSetLogicalSize(renderer, w, h);

    // Create texture that stores frame buffer
    SDL_Texture *sdlTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        64, 32);

    // Temporary pixel buffer
    uint32_t pixels[2048];

    // emulation loop
    while (true)
    {
        c8.executeCycle();

        // Process SDL events
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                exit(0);
            }
            // Process keydown events
            else if (e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.sym == SDLK_ESCAPE)
                    exit(0);

                if (e.key.keysym.sym == SDLK_F1)
                    c8.loadROMFromPath(argv[1]);

                for (int i = 0; i < 16; ++i)
                {
                    if (e.key.keysym.sym == keymap[i])
                    {
                        c8.keys[i] = 1;
                    }
                }
            }
            // Process keyup events
            else if (e.type == SDL_KEYUP)
            {
                for (int i = 0; i < 16; ++i)
                {
                    if (e.key.keysym.sym == keymap[i])
                    {
                        c8.keys[i] = 0;
                    }
                }
            }
        }

        // draw to the screen
        if (c8.draw)
        {
            c8.draw = false;
            for (size_t i = 0; i < (32 * 64); i++)
            {
                size_t byteIndex = i / 8;
                size_t byteShamt = 7 - (i % 8);
                int on = ((c8.display[byteIndex]) & (1 << byteShamt)) >> byteShamt;
                pixels[i] = on ? 0x00FFFFFF : 0xFF000000;
            }
            // Update SDL texture
            SDL_UpdateTexture(sdlTexture, NULL, pixels, 64 * sizeof(Uint32));
            // Clear screen and render
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, sdlTexture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }

        // Sleep to slow down emulation speed
        std::this_thread::sleep_for(std::chrono::microseconds(1200));
    }

    SDL_DestroyRenderer(renderer);

    return 0;
}
