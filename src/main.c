#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ops.h"
#include "machine.h"
#include "debug.h"

void (*ops[256])(Machine* m) = {
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

    [0xFF] = HLT
};

void step(Machine* m) {
    uint8_t op = fetch8(m);
    ops[op](m);
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
    m.cpu.pc = 0;
    m.cpu.sp = MEM_SIZE;
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