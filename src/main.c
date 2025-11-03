#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ops.h"
#include "machine.h"
#include "debug.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define INT_MEM 0x100
#define MAX_INTERRUPTS 128  /* tune as needed */

/* opcode table */
void (*ops_table[256])(Machine* m) = {
    [0x00] = NOP,
    [0x01] = ADD,
    [0x02] = SUB,
    [0x03] = MUL,
    [0x04] = DIV,
    [0x05] = MOD,

    [0x10] = LDI,
    [0x11] = LDR,
    [0x12] = STR,
    [0x13] = MOV,

    [0x20] = CMP,
    [0x21] = JMP,
    [0x22] = JZ,
    [0x23] = JNZ,

    [0x30] = PUSH,
    [0x31] = POP,
    [0x32] = CALL,
    [0x33] = RET,

    [0x40] = IRET,
    [0x41] = STI,
    [0x42] = CLI,

    [0xFF] = HLT
};

/* keep track of how many interrupts we've added */
static size_t num_ints = 0;

/* push/pop 16-bit values to stack (stack grows down). Assumes sp is an index into m->mem and MEM_SIZE is available */
void push16(Machine* m, uint16_t value) {
    if (m->cpu.sp < 2) {
        fprintf(stderr, "Stack overflow (sp=%u)\n", (unsigned)m->cpu.sp);
        m->cpu.running = false;
        return;
    }
    m->cpu.sp -= 2;
    /* little-endian store: low byte then high byte */
    m->mem[m->cpu.sp    ] = (uint8_t)(value & 0xFF);
    m->mem[m->cpu.sp + 1] = (uint8_t)((value >> 8) & 0xFF);
}

uint16_t pop16(Machine* m) {
    if (m->cpu.sp > MEM_SIZE - 2) {
        fprintf(stderr, "Stack underflow (sp=%u)\n", (unsigned)m->cpu.sp);
        m->cpu.running = false;
        return 0;
    }
    uint16_t lo = m->mem[m->cpu.sp];
    uint16_t hi = m->mem[m->cpu.sp + 1];
    m->cpu.sp += 2;
    return (uint16_t)((hi << 8) | lo);
}

/* single CPU step: either handle interrupt or fetch/execute one opcode */
void step(Machine* m) {
    if (m->cpu.flags.I && m->cpu.flags.IE) {
        /* validate interrupt number and vector table bounds */
        if (m->cpu.interrupt >= 256) {
            fprintf(stderr, "Invalid interrupt number %u\n", (unsigned)m->cpu.interrupt);
            m->cpu.running = false;
            return;
        }
        size_t vec_addr = INT_MEM + (size_t)m->cpu.interrupt * 2; /* 2 bytes per vector (little-endian) */
        if (vec_addr + 1 >= MEM_SIZE) {
            fprintf(stderr, "Interrupt vector out of range (vec_addr=0x%zx)\n", vec_addr);
            m->cpu.running = false;
            return;
        }

        /* push PC and jump to vector */
        push16(m, m->cpu.pc);
        uint16_t new_pc = (uint16_t)m->mem[vec_addr] | ((uint16_t)m->mem[vec_addr + 1] << 8);
        m->cpu.pc = new_pc;
        m->cpu.flags.I = false;
        return;
    }

    uint8_t op = fetch8(m);
    void (*handler)(Machine*) = ops_table[op];
    if (handler) {
        handler(m);
    } else {
        fprintf(stderr, "Illegal opcode 0x%02X at PC=0x%04X\n", op, m->cpu.pc);
        m->cpu.running = false;
    }
}

/* store a little-endian 16-bit interrupt vector at INT_MEM + 2 * index.
   Returns 0 on success, -1 on error. */
int add_int_vector(Machine* m, size_t index, uint16_t addr) {
    if (index >= MAX_INTERRUPTS) {
        fprintf(stderr, "Exceeded maximum interrupt entries (%zu)\n", (size_t)MAX_INTERRUPTS);
        return -1;
    }
    size_t base = INT_MEM + index * 2;
    if (base + 1 >= MEM_SIZE) {
        fprintf(stderr, "Interrupt table write out of bounds (base=0x%zx)\n", base);
        return -1;
    }
    /* little-endian: low byte first */
    m->mem[base    ] = (uint8_t)(addr & 0xFF);
    m->mem[base + 1] = (uint8_t)((addr >> 8) & 0xFF);
    return 0;
}

/* load binary file into memory at addr. Returns number of bytes loaded, or (size_t)-1 on error */
size_t load_file_to_mem(Machine* m, const char* file, uint16_t addr) {
    FILE* f = fopen(file, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open file %s\n", file);
        return (size_t)-1;
    }

    if (addr >= MEM_SIZE) {
        fprintf(stderr, "Start address 0x%04x out of bounds\n", addr);
        fclose(f);
        return (size_t)-1;
    }

    size_t remaining = MEM_SIZE - addr;
    size_t total = 0;
    while (total < remaining) {
        size_t want = remaining - total;
        if (want > 4096) want = 4096;
        size_t got = fread(&m->mem[addr + total], 1, want, f);
        if (got == 0) {
            if (ferror(f)) {
                fprintf(stderr, "Error reading from %s\n", file);
                fclose(f);
                return (size_t)-1;
            }
            break; /* EOF */
        }
        total += got;
    }

    if (!feof(f) && total == remaining) {
        fprintf(stderr, "Warning: input file %s truncated to %zu bytes at addr 0x%04x\n", file, total, addr);
    }

    fclose(f);
    return total;
}

/* convenience to register and load multiple interrupts */
typedef struct { uint16_t addr; const char* file; } IntDef;
int add_ints(Machine* m, const IntDef defs[], size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (num_ints >= MAX_INTERRUPTS) {
            fprintf(stderr, "Cannot add interrupt %zu: table full\n", i);
            return -1;
        }
        if (INT_MEM + num_ints * 2 + 1 >= MEM_SIZE) {
            fprintf(stderr, "Interrupt table overflow at index %zu\n", num_ints);
            return -1;
        }
        if (add_int_vector(m, num_ints, defs[i].addr) != 0) return -1;

        if (defs[i].file) {
            size_t got = load_file_to_mem(m, defs[i].file, defs[i].addr);
            if (got == (size_t)-1) return -1;
        }
        num_ints++;
    }
    return 0;
}

int main(int argc, char** argv) {
    Machine m;
    memset(&m, 0, sizeof(m));

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program.bin>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* load target program */
    FILE* target = fopen(argv[1], "rb");
    if (!target) {
        fprintf(stderr, "Failed to open target program %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    /* read entire file into memory (up to MEM_SIZE) */
    size_t loaded = 0;
    while (loaded < MEM_SIZE) {
        size_t want = MEM_SIZE - loaded;
        if (want > 4096) want = 4096;
        size_t got = fread(&m.mem[loaded], 1, want, target);
        if (got == 0) {
            if (ferror(target)) {
                fprintf(stderr, "Error reading target program %s\n", argv[1]);
                fclose(target);
                return EXIT_FAILURE;
            }
            break;
        }
        loaded += got;
    }
    fclose(target);

    /* example interrupt(s) to add */
    IntDef ints[] = {
        { .addr = 0x6969, .file = "build/ints/timer" }
    };
    if (add_ints(&m, ints, sizeof(ints) / sizeof(*ints)) != 0) {
        fprintf(stderr, "Failed to register interrupts\n");
        return EXIT_FAILURE;
    }

    /* initialize CPU */
    m.cpu.pc = 0;
    m.cpu.sp = MEM_SIZE;
    m.cpu.flags.I = false;
    m.cpu.flags.IE = true;
    m.cpu.running = true;
    m.cpu.cycle = 0;

    /* SDL/TTF init with error checks */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Window* win = SDL_CreateWindow("Emu debug",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       1280, 720,
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Regular.ttf", 16);
    if (!font) {
        fprintf(stderr, "Failed to open font\n");
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Event e;
    int running = 1;
    int autorun = 0;           /* 0 = step-by-step, 1 = automatic */
    const Uint32 AUTODELAY = 0; /* ms between automatic steps */

    while (m.cpu.running && running) {
        draw_debug(&m, ren, font);

        if (autorun) {
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) { running = 0; break; }
                if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_ESCAPE) { running = 0; break; }
                    if (e.key.keysym.sym == SDLK_SPACE) { autorun = !autorun; break; }
                }
            }
            if (!running) break;

            if ((m.cpu.cycle++ % 1000) == 0) {
                m.cpu.flags.I = true;
                m.cpu.interrupt = 0;
            }
            step(&m);
            draw_debug(&m, ren, font);
            if (AUTODELAY) SDL_Delay(AUTODELAY);
        } else {
            while (SDL_WaitEvent(&e)) {
                if (e.type == SDL_QUIT) { running = 0; break; }
                if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_ESCAPE) { running = 0; break; }
                    if (e.key.keysym.sym == SDLK_SPACE) { autorun = !autorun; break; }
                    /* any other key advances one step */
                    if ((m.cpu.cycle++ % 1000) == 0) {
                        m.cpu.flags.I = true;
                        m.cpu.interrupt = 0;
                    }
                    step(&m);
                    draw_debug(&m, ren, font);
                    break;
                }
            }
        }
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();

    return m.cpu.running ? EXIT_SUCCESS : EXIT_FAILURE;
}
