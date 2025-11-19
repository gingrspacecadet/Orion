#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "vga.h"
#include "font.h"
#include "device.h"

vga_State* vga_init(char* title, size_t width, size_t height) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return NULL;

    vga_State* vga = (vga_State*)malloc(sizeof(vga_State));
    if (!vga) {
        SDL_Quit();
        return NULL;
    }

    vga->width = (int)width;
    vga->height = (int)height;

    vga->window = SDL_CreateWindow(title,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  vga->width,
                                  vga->height,
                                  SDL_WINDOW_SHOWN);
    if (!vga->window) {
        free(vga);
        SDL_Quit();
        return NULL;
    }

    vga->surface = SDL_GetWindowSurface(vga->window);
    if (!vga->surface) {
        SDL_DestroyWindow(vga->window);
        free(vga);
        SDL_Quit();
        return NULL;
    }

    return vga;
}

void vga_putpixel(vga_State* vga, int x, int y, uint32_t colour) {
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

void vga_parse(uint16_t data) {

}


void vga_drawchar(vga_State* vga, Font* c, int dst_x, int dst_y) {
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

void vga_render(vga_State* vga, Machine* m, uint32_t addr) {
    if (!vga || !vga->window || !vga->surface) return;

    SDL_LockSurface(vga->surface);

    int cols = vga->width / CHAR_W;
    int rows = vga->height / CHAR_H;

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            vga_drawchar(vga, vga_getchar(getbyte(bus_read(m, VGA_BASE + y * cols + x), 32)), x * CHAR_W, y * CHAR_H);
        }
    }

    SDL_UnlockSurface(vga->surface);
    SDL_UpdateWindowSurface(vga->window);
}

void vga_destroy(vga_State* vga) {
    if (!vga) return;
    if (vga->window) SDL_DestroyWindow(vga->window);
    SDL_Quit();
    free(vga);
}
