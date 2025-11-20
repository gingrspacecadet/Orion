#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "vga.h"
#include "font.h"
#include "device.h"

void vga_init(Device* self) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return;

    VGA* vga = (VGA*)malloc(sizeof(VGA));
    if (!vga) {
        SDL_Quit();
        return;
    }

    vga->width = (int)VGA_W;
    vga->height = (int)VGA_H;
    vga->mem = malloc(sizeof(uint32_t) * VGA_H * VGA_W);

    vga->window = SDL_CreateWindow("Orion",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  vga->width,
                                  vga->height,
                                  SDL_WINDOW_SHOWN);
    if (!vga->window) {
        free(vga);
        free(vga->mem);
        SDL_Quit();
        return;
    }

    vga->surface = SDL_GetWindowSurface(vga->window);
    if (!vga->surface) {
        SDL_DestroyWindow(vga->window);
        free(vga);
        free(vga->mem);
        SDL_Quit();
        return;
    }

    self->state = vga;
}

void vga_putpixel(VGA* vga, int x, int y, uint32_t colour) {
    if (!vga || !vga->window || !vga->surface || !vga->surface->locked) return;

    SDL_PixelFormat *fmt = vga->surface->format;
    int rloss = fmt->Rloss, gloss = fmt->Gloss, bloss = fmt->Bloss, aloss = fmt->Aloss;
    int rshift = fmt->Rshift, gshift = fmt->Gshift, bshift = fmt->Bshift, ashift = fmt->Ashift;

        uint8_t *dst_row = (uint8_t*)vga->surface->pixels + y * vga->surface->pitch;
        uint32_t *dst = (uint32_t*)dst_row;
        uint32_t s = colour;
        uint32_t r = (s >> 24) & 0xFF;
        uint32_t g = (s >> 16) & 0xFF;
        uint32_t b = (s >> 8)  & 0xFF;
        uint32_t a = 0xFF;

        uint32_t packed =
            ((r >> rloss) << rshift) |
            ((g >> gloss) << gshift) |
            ((b >> bloss) << bshift) |
            ((a >> aloss) << ashift);

        dst[x] = packed;

}

void vga_drawchar(VGA* vga, Font* c, int dst_x, int dst_y) {
    if (!vga || !c || !c->val) return;

    for (int y = 0; y < CHAR_H; ++y) {
        for (int x = 0; x < CHAR_W; ++x) {
            if (c->val[y * CHAR_W + x] != '.') {
                vga_putpixel(vga, dst_x + x, dst_y + y, 0xFFFFFF00);
            }
        }
    }
}

Font* vga_getchar(char c) {
    for(size_t i = 0; i < 128; i++)
        if(font[i].c == c) return &font[i];
    return NULL;
}

void vga_render(VGA* vga) {
    if (!vga || !vga->window || !vga->surface) return;

    SDL_LockSurface(vga->surface);

    int cols = vga->width / CHAR_W;
    int rows = vga->height / CHAR_H;

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            vga_drawchar(vga, vga_getchar(getbyte(vga->mem[y * cols + x], 32)), x * CHAR_W, y * CHAR_H);
        }
    }

    SDL_UnlockSurface(vga->surface);
    SDL_UpdateWindowSurface(vga->window);
}

void vga_destroy(VGA* vga) {
    if (!vga) return;
    if (vga->window) SDL_DestroyWindow(vga->window);
    if (vga->mem) free(vga->mem);
    SDL_Quit();
    free(vga);
}

void vga_write(Device* self, uint32_t addr, uint32_t value) {
    VGA* vga = self->state;
    switch (addr) {
        case 0x0: {
            vga->addr = value;
            break;
        }
        case 0x1: {
            vga->mem[vga->addr] = value;
            vga_render(vga);
        }
    }
}

VGA vga_state;

Device vga_device = (Device){
    .read = NULL,
    .write = vga_write,
    .base = 0xB8000,
    .size = 0x1,
    .state = &vga_state,
    .init = vga_init
};