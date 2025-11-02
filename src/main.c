#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ops.h"
#include "machine.h"

void (*ops[256])(Machine* m) = {
    [0x01] = ADD,
    [0x02] = SUB,
    [0x03] = MUL,
    [0x04] = DIV,
    [0x05] = MOD,
    [0x10] = LDI,
    [0xFF] = HLT
};

void step(Machine* m) {
    uint8_t op = fetch8(m);
    ops[op](m);
}

/* DEBUG START */
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

void draw_debug(Machine* m, SDL_Renderer* renderer, TTF_Font* font) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer); 

    char buf[64];
    SDL_Color white = {255, 255, 255, 255};
    for (int i = 0; i < 8; i++) {
        snprintf(buf, sizeof(buf), "R%d = %02X", i, m->cpu.reg[i]);
        SDL_Surface* surf = TTF_RenderText_Solid(font, buf, white);
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_Rect dst = {20, 20 + i*20, surf->w, surf->h};
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    }

    SDL_RenderPresent(renderer);
}

/* DEBUG END */

int main(void) {
    Machine m = {0};
    uint8_t program[] = {
        0x10, 0x01, 0x08,   // LDI R1, 8
        0x10, 0x02, 0x04,   // LDI R2, 4
        0x01, 0x01, 0x02,   // ADD R1, R2
        0x02, 0x01, 0x02,   // SUB R1, R2
        0xFF                // HLT
    };
    for (size_t i = 0; i < sizeof(program); i++) { m.mem[i] = program[i]; }
    m.cpu.pc = 0;
    m.cpu.running = true;

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window* win = SDL_CreateWindow("Emu debug", 100, 100, 300, 300, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, 0);
    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Regular.ttf", 16);

    while (m.cpu.running) {
        step(&m);
        draw_debug(&m, ren, font);
        SDL_Delay(500);
    }

    SDL_Quit();
    return 0;
}