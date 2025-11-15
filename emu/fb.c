#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "machine.h"
#include "fb.h"

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

void fb_render(fb_State* fb, uint32_t* data) {
    if (!fb || !fb->window || !fb->surface) return;
    SDL_LockSurface(fb->surface);

    SDL_PixelFormat *fmt = fb->surface->format;
    int rloss = fmt->Rloss, gloss = fmt->Gloss, bloss = fmt->Bloss, aloss = fmt->Aloss;
    int rshift = fmt->Rshift, gshift = fmt->Gshift, bshift = fmt->Bshift, ashift = fmt->Ashift;

    for (int y = 0; y < fb->height; ++y) {
        uint8_t *dst_row = (uint8_t*)fb->surface->pixels + y * fb->surface->pitch;
        uint32_t *dst = (uint32_t*)dst_row;
        const uint32_t *src = data + (size_t)y * fb->width;
        for (int x = 0; x < fb->width; ++x) {
            uint32_t s = src[x];
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
