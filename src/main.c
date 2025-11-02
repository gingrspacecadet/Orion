#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ops.h"
#include "machine.h"
#include "debug.h"

#define INT_MEM 0x100

void (*ops[256])(Machine* m) = {
    [0x00] = NOP,
    [0x01] = ADD,
    [0x02] = SUB,
    [0x03] = MUL,
    [0x04] = DIV,
    [0x05] = MOD,

    [0x10] = LDI,
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

    [0xFF] = HLT
};

/* assume MEM_SIZE and Machine definition use uint16_t PC and uint16_t sp */
void push16(Machine* m, uint16_t value) {
    if (m->cpu.sp < 2) { fprintf(stderr, "Stack overflow\n"); m->cpu.running = false; return; }
    m->cpu.sp -= 2;
    m->mem[m->cpu.sp    ] = (uint8_t)(value & 0xFF);
    m->mem[m->cpu.sp + 1] = (uint8_t)((value >> 8) & 0xFF);
}

uint16_t pop16(Machine* m) {
    if (m->cpu.sp > MEM_SIZE - 2) { fprintf(stderr, "Stack underflow\n"); m->cpu.running = false; return 0; }
    uint16_t lo = m->mem[m->cpu.sp];
    uint16_t hi = m->mem[m->cpu.sp + 1];
    m->cpu.sp += 2;
    return (uint16_t)((hi << 8) | lo);
}

void step(Machine* m) {
    if (m->cpu.flags.IE) {
        /* validate interrupt number and vector table bounds */
        if (m->cpu.interrupt >= 256) { fprintf(stderr, "Invalid interrupt %u\n", m->cpu.interrupt); m->cpu.running = false; return; }
        size_t vec_addr = INT_MEM + (size_t)m->cpu.interrupt * 2; /* assume vector table is 2 bytes per entry */
        if (vec_addr + 1 >= MEM_SIZE) { fprintf(stderr, "Interrupt vector out of range\n"); m->cpu.running = false; return; }

        /* push full PC, switch PC to vector (little-endian) */
        push16(m, m->cpu.pc);
        uint16_t new_pc = (uint16_t)m->mem[vec_addr] | ((uint16_t)m->mem[vec_addr + 1] << 8);
        m->cpu.pc = new_pc;
        m->cpu.flags.IE = false;
    } else {
        uint8_t op = fetch8(m);
        if (ops[op]) ops[op](m);
        else { fprintf(stderr, "Illegal opcode 0x%02X at PC=0x%04X\n", op, m->cpu.pc); m->cpu.running = false; }
    }
}

int num_ints = 0;

void add_int(Machine* m, uint16_t addr) {
    m->mem[INT_MEM + num_ints] = (uint8_t)((addr & 0xFF00) >> 8);
    m->mem[INT_MEM + num_ints + 1] = (uint8_t)(addr & 0x00FF);
    num_ints++;
}


void int_code(Machine *m, const char *file, uint16_t addr) {
    FILE *src = fopen(file, "rb");
    if (!src) {
        fprintf(stderr, "Failed to open int source file %s\n", file);
        exit(EXIT_FAILURE);
    }

    if (addr >= MEM_SIZE) {
        fprintf(stderr, "Start address 0x%04x out of bounds\n", addr);
        fclose(src);
        exit(EXIT_FAILURE);
    }

    size_t remaining = MEM_SIZE - addr;
    size_t got = fread(&m->mem[addr], 1, remaining, src);
    if (got == 0 && ferror(src)) {
        fprintf(stderr, "Error reading from %s\n", file);
        fclose(src);
        exit(EXIT_FAILURE);
    }

    if (!feof(src) && got == remaining) {
        fprintf(stderr, "Warning: input file %s truncated to %zu bytes at addr 0x%04x\n", file, remaining, addr);
    }

    fclose(src);
}

typedef struct { uint16_t addr; char* file } Int;

void add_ints(Machine* m, Int interrupt[], size_t num) {
    for (int i = 0; i < num; i++) {
        add_int(m, interrupt[i].addr);
        int_code(m, interrupt[i].file, interrupt[i].addr);
    }
}

int main(int argc, char** argv) {
    Machine m = {0};
    
    if (argc < 2) {
        fprintf(stderr, "Missing target program\n");
        return 1;
    }

    FILE* target = fopen(argv[1], "rb");
    uint8_t program[1024];
    fread(program, sizeof(program), 1, target);
    
    for (size_t i = 0; i < sizeof(program); i++) { m.mem[i] = program[i]; }

    add_ints(&m, (Int[])
    {
        {
            (uint16_t)0x6969,
            "build/ints/test.int"
        }
    }, 1);
    // add_int(&m, 0x6969);
    // int_code(&m, "build/ints/test.int", 0x6969);

    m.cpu.pc = 0;
    m.cpu.sp = MEM_SIZE;
    m.cpu.flags.IE = false;
    m.cpu.running = true;

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window* win = SDL_CreateWindow("Emu debug",
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   1280, 720,
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_FULLSCREEN);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, 0);
    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Regular.ttf", 16);

    SDL_Event e;
    int running = 1;
    int autorun = 0;           /* 0 = step-by-step, 1 = automatic */
    const Uint32 AUTODELAY = 0; /* ms between automatic steps */

    int i = 0;
    while (m.cpu.running && running) {
        draw_debug(&m, ren, font);

        if (autorun) {
            /* In autorun mode: poll events without blocking, step periodically */
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) { running = 0; break; }
                if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_ESCAPE) { running = 0; break; }
                    if (e.key.keysym.sym == SDLK_SPACE) { autorun = !autorun; break; }
                }
            }
            if (!running) break;

            step(&m);
            draw_debug(&m, ren, font);
            SDL_Delay(AUTODELAY);
        } else {
            /* Step-by-step: block until an event arrives */
            while (SDL_WaitEvent(&e)) {
                if (e.type == SDL_QUIT) { running = 0; break; }
                if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_ESCAPE) { running = 0; break; }
                    if (e.key.keysym.sym == SDLK_SPACE) { autorun = !autorun; break; }
                    /* any other key advances one step */
                    if (i++ % 5 == 0) {
                        m.cpu.flags.IE = true;
                        m.cpu.interrupt = 0;
                    }
                    step(&m);
                    draw_debug(&m, ren, font);
                    break;
                }
            }
        }
    }

    SDL_Quit();
    return 0;
}