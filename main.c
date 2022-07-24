#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "SDL2/SDL.h"

enum constants
{
    MEM_NB = 4096,
    FONTSET_NB = 80,
    PROG_START = 0x200
};

union chip8_t
{
    uint8_t memory[MEM_NB];
    struct
    {
        // first 80 bytes reserved for fontset
        uint8_t fontset[FONTSET_NB];

        uint8_t V[16]; // registers
        uint8_t DT;    // delay timer
        uint8_t ST;    // sound timer
        uint8_t SP;    // stack pointer

        uint16_t PC;        // program counter
        uint16_t I;         // I register
        uint16_t stack[16]; // call stack

        // 32 x 64 = 2048 bit (256 byte) screen
        // we emulate this via a bit array to save memory
        uint8_t display[32 * 64 / 8];

        // whether to draw the screen
        // not a part of chip 8 but reduces emulator flickering
        uint8_t draw_flag;

        // represents whether a key is pressed
        // TODO: use a single uint_16 here as a bit array
        // to save space
        uint8_t keys[16];

        // in this implementation, the interpreter uses
        // 80 + 16 + 1 + 1 + 1 + 2 + 2 + 32 + 256 + 1 + 16 = 408 bytes.
        // this is less than the program start (0x200 = 512), so
        // it's safe to manipulate this part of the chip 8's memory
    };
};

const uint8_t FONTSET[FONTSET_NB] = {
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

void chip8_t_emulate_cycle(union chip8_t *c8)
{
    // instructions are two bytes long
    const uint16_t instruction = (c8->memory[c8->PC] << 8) | c8->memory[c8->PC + 1];
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
            memset(c8->display, 0, 32 * 64 / 8);
            c8->draw_flag = 1;
            c8->PC += 2;
        }
        // 00EE - RET
        else if (n == 0xE)
        {
            // Return from a subroutine.
            // The interpreter sets the program counter to the address at
            // the top of the stack, then subtracts 1 from the stack pointer.
            c8->SP -= 1;
            c8->PC = c8->stack[c8->SP];
            c8->PC += 2;
        }
    }
    // 1nnn - JP addr
    else if (op == 0x1)
    {
        // Jump to location nnn.
        // The interpreter sets the program counter to nnn.
        c8->PC = nnn;
    }
    // 2nnn - CALL addr
    else if (op == 0x2)
    {
        // Call subroutine at nnn.
        // The interpreter increments the stack pointer, then puts
        // the current PC on the top of the stack.
        // The PC is then set to nnn.
        c8->stack[c8->SP] = c8->PC;
        c8->SP += 1;
        c8->PC = nnn;
    }
    // 3xkk - SE Vx, byte
    else if (op == 0x3)
    {
        // Skip next instruction if Vx = kk.
        // The interpreter compares register Vx to kk, and if they are equal,
        // increments the program counter by 2.
        c8->PC += (c8->V[x] == kk) ? 4 : 2;
    }
    // 4xkk - SNE Vx, byte
    else if (op == 0x4)
    {
        // Skip next instruction if Vx != kk.
        // The interpreter compares register Vx to kk, and if they
        // are not equal, increments the program counter by 2.
        c8->PC += (c8->V[x] != kk) ? 4 : 2;
    }
    // 5xy0 - SE Vx, Vy
    else if (op == 0x5)
    {
        // Skip next instruction if Vx = Vy.
        // The interpreter compares register Vx to register Vy, and if they
        // are equal, increments the program counter by 2.
        c8->PC += (c8->V[x] == c8->V[y]) ? 4 : 2;
    }
    // 6xkk - LD Vx, byte
    else if (op == 0x6)
    {
        // Set Vx = kk.
        // The interpreter puts the value kk into register Vx.
        c8->V[x] = kk;
        c8->PC += 2;
    }
    // 7xkk - ADD Vx, byte
    else if (op == 0x7)
    {
        // Set Vx = Vx + kk.
        // Adds the value kk to the value of register Vx,
        // then stores the result in Vx.
        c8->V[x] += kk;
        c8->PC += 2;
    }
    else if (op == 0x8)
    {
        // 8xy0 - LD Vx, Vy
        if (n == 0x0)
        {
            // Set Vx = Vy.
            // Stores the value of register Vy in register Vx.
            c8->V[x] = c8->V[y];
            c8->PC += 2;
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
            c8->V[x] |= c8->V[y];
            c8->PC += 2;
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
            c8->V[x] &= c8->V[y];
            c8->PC += 2;
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
            c8->V[x] ^= c8->V[y];
            c8->PC += 2;
        }
        // 8xy4 - ADD Vx, Vy
        else if (n == 0x4)
        {
            // Set Vx = Vx + Vy, set VF = carry.
            // The values of Vx and Vy are added together.
            // If the result is greater than 8 bits (i.e., > 255,) VF is
            // set to 1, otherwise 0.
            // Only the lowest 8 bits of the result are kept, and stored in Vx.

            c8->V[0xF] = (c8->V[x] + c8->V[y] > 255) ? 1 : 0;
            c8->V[x] += c8->V[y];
            c8->PC += 2;
        }
        // 8xy5 - SUB Vx, Vy
        else if (n == 0x5)
        {
            // Set Vx = Vx - Vy, set VF = NOT borrow.
            // If Vx > Vy, then VF is set to 1, otherwise 0.
            // Then Vy is subtracted from Vx, and the results stored in Vx.
            c8->V[0xF] = (c8->V[x] > c8->V[y]) ? 1 : 0;
            c8->V[x] -= c8->V[y];
            c8->PC += 2;
        }
        // 8xy6 - SHR Vx {, Vy}
        else if (n == 0x6)
        {
            // Set Vx = Vx SHR 1.
            // If the least-significant bit of Vx is 1, then VF is set to 1,
            // otherwise 0.
            // Then Vx is divided by 2.
            c8->V[0xF] = c8->V[x] & 1;
            c8->V[x] >>= 1;
            c8->PC += 2;
        }
        // 8xy7 - SUBN Vx, Vy
        else if (n == 0x7)
        {
            // Set Vx = Vy - Vx, set VF = NOT borrow.
            // If Vy > Vx, then VF is set to 1, otherwise 0.
            // Then Vx is subtracted from Vy, and the results stored in Vx.
            c8->V[0xF] = ((c8->V[y]) > (c8->V[x])) ? 1 : 0;
            c8->V[x] = (c8->V[y]) - (c8->V[x]);
            c8->PC += 2;
        }
        // 8xyE - SHL Vx {, Vy}
        else if (n == 0xE)
        {
            // Set Vx = Vx SHL 1.
            // If the most-significant bit of Vx is 1, then VF is set to 1,
            // otherwise to 0.
            // Then Vx is multiplied by 2.
            c8->V[0xF] = (c8->V[x] >> 7);
            c8->V[x] <<= 1;
            c8->PC += 2;
        }
    }
    // 9xy0 - SNE Vx, Vy
    else if (op == 0x9)
    {
        // Skip next instruction if Vx != Vy.
        // The values of Vx and Vy are compared, and if they are not equal,
        // the program counter is increased by 2.
        c8->PC += (c8->V[x] != c8->V[y]) ? 4 : 2;
    }
    // Annn - LD I, addr
    else if (op == 0xA)
    {
        // Set I = nnn.
        // The value of register I is set to nnn.
        c8->I = nnn;
        c8->PC += 2;
    }
    // Bnnn - JP V0, addr
    else if (op == 0xB)
    {
        // Jump to location nnn + V0.
        // The program counter is set to nnn plus the value of V0.
        c8->PC = nnn + c8->V[0];
    }
    // Cxkk - RND Vx, byte
    else if (op == 0xC)
    {
        // Set Vx = random byte AND kk.
        // The interpreter generates a random number from 0 to 255,
        // which is then ANDed with the value kk.
        // The results are stored in Vx.
        // See instruction 8xy2 for more information on AND.
        c8->V[x] = ((rand() % 256) & kk);
        c8->PC += 2;
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
        c8->V[0xF] = 0;

        size_t startCol = c8->V[x];
        size_t startRow = c8->V[y];

        // read each byte
        for (size_t offsetRow = 0; offsetRow < n; offsetRow++)
        {
            uint8_t nthByte = c8->memory[c8->I + offsetRow];
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
                uint8_t originalPxVal = ((c8->display[byteIndex]) & (1 << dispShamt)) >> dispShamt;
                uint8_t newPxVal = originalPxVal ^ bytePxVal;

                // set the new value
                c8->display[byteIndex] ^= ((bytePxVal) << dispShamt);

                // update VF
                c8->V[0xF] = c8->V[0xF] | ((originalPxVal && (!newPxVal)) ? 1 : 0);
            }
        }

        c8->draw_flag = 1;
        c8->PC += 2;
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
            c8->PC += (c8->keys[c8->V[x]]) ? 4 : 2;
        }
        // ExA1 - SKNP Vx
        else if (kk == 0xA1)
        {
            // Skip next instruction if key with the val of Vx is not pressed.
            // Checks the keyboard, and if the key corresponding to
            // the value of Vx is currently in the up position,
            // PC is increased by 2.
            c8->PC += (!c8->keys[c8->V[x]]) ? 4 : 2;
        }
    }
    else if (op == 0xF)
    {
        // Fx07 - LD Vx, DT
        if (kk == 0x07)
        {
            // Set Vx = delay timer value.
            // The value of DT is placed into Vx.
            c8->V[x] = c8->DT;
            c8->PC += 2;
        }
        // Fx0A - LD Vx, K
        else if (kk == 0x0A)
        {
            // Wait for a key press, store the value of the key in Vx.
            // All execution stops until a key is pressed, then the value of that key is stored in Vx.

            // check if a key was pressed, and if not,
            // then perform this instruction again
            uint8_t keyPressed = 0;
            for (size_t i = 0; i < 16; i++)
            {
                keyPressed = keyPressed || c8->keys[i];
                c8->V[x] = keyPressed ? i : (c8->V[x]);
            }

            // don't increment PC if key not pressed
            if (!keyPressed)
            {
                return;
            }

            c8->PC += 2;
        }
        // Fx15 - LD DT, Vx
        else if (kk == 0x15)
        {
            // Set delay timer = Vx.
            // DT is set equal to the value of Vx.
            c8->DT = c8->V[x];
            c8->PC += 2;
        }
        // Fx18 - LD ST, Vx
        else if (kk == 0x18)
        {
            // Set sound timer = Vx.
            // ST is set equal to the value of Vx.
            c8->ST = c8->V[x];
            c8->PC += 2;
        }
        // Fx1E - ADD I, Vx
        else if (kk == 0x1E)
        {
            // Set I = I + Vx.
            // The values of I and Vx are added,
            // and the results are stored in I.
            // VF is set to 1 when range overflow (I+VX>0xFFF),
            // and 0 when it isn't
            c8->V[0xF] = (c8->I + c8->V[x]) > 0xFFF;
            c8->I += c8->V[x];
            c8->PC += 2;
        }
        else if (kk == 0x29)
        {
            // Fx29 - LD F, Vx
            // Set I = location of sprite for digit Vx.
            // The value of I is set to the location for
            // the hexadecimal sprite corresponding to the value of Vx.
            // See section 2.4, Display, for more information on
            // the Chip-8 hexadecimal font.
            c8->I = 5 * c8->V[x];
            c8->PC += 2;
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
            c8->memory[c8->I] = c8->V[x] / 100;
            c8->memory[c8->I + 1] = (c8->V[x] / 10) % 10;
            c8->memory[c8->I + 2] = c8->V[x] % 10;
            c8->PC += 2;
        }
        // Fx55 - LD [I], Vx
        else if (kk == 0x55)
        {
            // Store registers V0 through Vx in memory starting at location I.
            // The interpreter copies the values of registers V0 through Vx
            // into memory, starting at the address in I.
            memcpy(c8->memory + c8->I, c8->V, x + 1);
            c8->I += x + 1;
            c8->PC += 2;
        }
        // Fx65 - LD Vx, [I]
        else if (kk == 0x65)
        {
            // Read registers V0 through Vx from memory starting at location I.
            // The interpreter reads values from memory starting at location I
            // into registers V0 through Vx.
            memcpy(c8->V, ((c8->memory) + (c8->I)), x + 1);
            c8->I += x + 1;
            c8->PC += 2;
        }
    }

    // update timers
    if (c8->DT > 0)
    {
        c8->DT -= 1;
    }
    if (c8->ST > 0)
    {
        c8->ST -= 1;
    }
}

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
    union chip8_t c8;
    // clear memory
    // here is why I like C unions.
    // instead of initializing each struct member, we can
    // just set all the memory to zero!
    memset(c8.memory, 0, MEM_NB);
    // load font set
    memcpy(c8.fontset, FONTSET, FONTSET_NB);
    // initialize PC
    c8.PC = PROG_START;

    // load rom
    if (argc < 2)
    {
        fprintf(stderr, "USAGE: ./main ROM\n");
    }
    // TODO: Check for errors
    FILE *rom = fopen(argv[1], "rb");
    fread(c8.memory + PROG_START, sizeof(uint8_t), (MEM_NB - PROG_START), rom);
    fclose(rom);

    // set up SDL
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
    for (;;)
    {

        chip8_t_emulate_cycle(&c8);

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
                {
                    // TODO: find a graceful way to remove goto
                    goto end;
                }

                // TODO: Find a better way to do this than a loop
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
        if (c8.draw_flag != 0)
        {
            c8.draw_flag = 0;
            for (size_t i = 0; i < (32 * 64); i++)
            {
                size_t byteIndex = i / 8;
                size_t byteShamt = 7 - (i % 8);
                uint8_t on = ((c8.display[byteIndex]) & (1 << byteShamt)) >> byteShamt;
                pixels[i] = on ? 0x00FFFFFF : 0xFF000000;
            }
            // Update SDL texture
            SDL_UpdateTexture(sdlTexture, NULL, pixels, 64 * sizeof(uint32_t));
            // Clear screen and render
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, sdlTexture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }

        // Sleep to slow down emulation speed
        sleep(0.167);
    }

end:
    SDL_DestroyRenderer(renderer);

    return 0;
}
