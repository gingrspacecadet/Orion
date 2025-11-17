#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "machine.h"
#include "fb.h"
#include "font.h"

fb_State* fb_init(char* title, size_t width, size_t height) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return NULL;

    fb_State* fb = (fb_State*)malloc(sizeof(fb_State));
    if (!fb) {
        SDL_Quit();
        return NULL;
    }

    fb->width = (int)width;
    fb->height = (int)height;

    fb->window = SDL_CreateWindow(title,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  fb->width,
                                  fb->height,
                                  SDL_WINDOW_SHOWN);
    if (!fb->window) {
        free(fb);
        SDL_Quit();
        return NULL;
    }

    fb->surface = SDL_GetWindowSurface(fb->window);
    if (!fb->surface) {
        SDL_DestroyWindow(fb->window);
        free(fb);
        SDL_Quit();
        return NULL;
    }

    return fb;
}

void fb_putpixel(fb_State* fb, int x, int y, uint32_t colour) {
    if (!fb || !fb->window || !fb->surface || !fb->surface->locked) return;

    SDL_PixelFormat *fmt = fb->surface->format;
    int rloss = fmt->Rloss, gloss = fmt->Gloss, bloss = fmt->Bloss, aloss = fmt->Aloss;
    int rshift = fmt->Rshift, gshift = fmt->Gshift, bshift = fmt->Bshift, ashift = fmt->Ashift;

        uint8_t *dst_row = (uint8_t*)fb->surface->pixels + y * fb->surface->pitch;
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

void fb_parse(uint16_t data) {

}


void fb_drawchar(fb_State* fb, Font* c, int dst_x, int dst_y) {
    if (!fb || !c || !c->val) return;

    for (int y = 0; y < CHAR_H; ++y) {
        for (int x = 0; x < CHAR_W; ++x) {
            if (c->val[y * CHAR_W + x] != '.') {
                fb_putpixel(fb, dst_x + x, dst_y + y, 0xFFFFFF00);
            }
        }
    }
}

Font* fb_getchar(char c) {
    for(size_t i = 0; i < 128; i++)
        if(font[i].c == c) return &font[i];
    return NULL;
}

void fb_render(fb_State* fb, uint32_t* data) {
    if (!fb || !fb->window || !fb->surface) return;

    SDL_LockSurface(fb->surface);

    int cols = fb->width / CHAR_W;
    int rows = fb->height / CHAR_H;

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            fb_drawchar(fb, fb_getchar(getbyte(data[y * cols + x], 32)), x * CHAR_W, y * CHAR_H);
        }
    }

    SDL_UnlockSurface(fb->surface);
    SDL_UpdateWindowSurface(fb->window);
}

void fb_destroy(fb_State* fb) {
    if (!fb) return;
    if (fb->window) SDL_DestroyWindow(fb->window);
    SDL_Quit();
    free(fb);
}
