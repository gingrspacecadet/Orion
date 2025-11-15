#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "machine.h"
#include "fb.h"

/* Helper assumed to exist in original code:
   uint8_t getbyte(uint32_t v, int shift) { return (v >> shift) & 0xFF; }
   If your getbyte is different, keep using that. */

static inline void write_pixel_bpp(uint8_t *dst, int bpp, uint32_t pixel)
{
    switch (bpp) {
    case 1: dst[0] = (uint8_t)pixel; break;
    case 2: *(uint16_t*)dst = (uint16_t)pixel; break;
    case 3:
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            dst[0] = (pixel >> 16) & 0xFF;
            dst[1] = (pixel >> 8) & 0xFF;
            dst[2] = pixel & 0xFF;
        } else {
            dst[0] = pixel & 0xFF;
            dst[1] = (pixel >> 8) & 0xFF;
            dst[2] = (pixel >> 16) & 0xFF;
        }
        break;
    case 4: *(uint32_t*)dst = pixel; break;
    }
}

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

void fb_clear(fb_State* fb, uint8_t r, uint8_t g, uint8_t b) {
    if (!fb || !fb->surface) return;

    /* prefer SDL_FillRect (safe) but use fast path when 32-bit surface */
    if (fb->surface->format->BytesPerPixel == 4) {
        uint32_t color = SDL_MapRGB(fb->surface->format, r, g, b);
        /* fill by rows to respect pitch */
        uint32_t *row = (uint32_t*)fb->surface->pixels;
        int pixels_per_row = fb->surface->pitch / 4;
        for (int y = 0; y < fb->height; ++y) {
            for (int x = 0; x < fb->width && x < pixels_per_row; ++x)
                row[x] = color;
            row = (uint32_t*)((uint8_t*)row + fb->surface->pitch);
        }
    } else {
        uint32_t color = SDL_MapRGB(fb->surface->format, r, g, b);
        SDL_FillRect(fb->surface, NULL, color);
    }
}

void fb_set_pixel(fb_State* fb, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (!fb || !fb->surface) return;
    if ((unsigned)x >= (unsigned)fb->width || (unsigned)y >= (unsigned)fb->height) return;

    SDL_Surface *s = fb->surface;
    int bpp = s->format->BytesPerPixel;
    uint8_t *base = (uint8_t*)s->pixels + y * s->pitch + x * bpp;

    /* build pixel in native surface format quickly */
    uint32_t pixel;
    if (bpp == 4) {
        pixel = ((uint32_t)r << s->format->Rshift) >> (8 - s->format->Rloss);
        pixel |= ((uint32_t)g << s->format->Gshift) >> (8 - s->format->Gloss);
        pixel |= ((uint32_t)b << s->format->Bshift) >> (8 - s->format->Bloss);
        /* shift fields are positive or zero, above composes roughly in native representation;
           fall back to SDL_MapRGB if format is unusual */
        if (s->format->Rshift == 0 && s->format->Gshift == 0 && s->format->Bshift == 0)
            pixel = SDL_MapRGB(s->format, r, g, b);
    } else {
        pixel = SDL_MapRGB(s->format, r, g, b);
    }

    write_pixel_bpp(base, bpp, pixel);
}

void fb_render(fb_State* fb, uint32_t* data) {
    if (!fb || !fb->surface || !data) return;

    SDL_Surface *s = fb->surface;
    int width = fb->width;
    int height = fb->height;
    int bpp = s->format->BytesPerPixel;

    if (SDL_MUSTLOCK(s) && SDL_LockSurface(s) != 0) return;

    uint8_t *pixels = (uint8_t*)s->pixels;
    for (int y = 0; y < height; ++y) {
        uint8_t *row = pixels + y * s->pitch;
        for (int x = 0; x < width; ++x) {
            uint32_t v = data[(size_t)y * (size_t)width + (size_t)x];
            uint8_t r = getbyte(v, 32);
            uint8_t g = getbyte(v, 24);
            uint8_t b = getbyte(v, 16);

            /* Fast path: if 32-bit surface and its layout matches native RGBA/ARGB ordering,
               write composed 32-bit value directly. Otherwise map and write by bpp. */
            if (bpp == 4) {
                uint32_t pixel = SDL_MapRGB(s->format, r, g, b);
                *(uint32_t*)(row + x * 4) = pixel;
            } else {
                uint32_t pixel = SDL_MapRGB(s->format, r, g, b);
                write_pixel_bpp(row + x * bpp, bpp, pixel);
            }
        }
    }

    if (SDL_MUSTLOCK(s)) SDL_UnlockSurface(s);

    SDL_UpdateWindowSurface(fb->window);
}

void fb_destroy(fb_State* fb) {
    if (!fb) return;
    if (fb->window) SDL_DestroyWindow(fb->window);
    /* don't need to free surface; SDL_DestroyWindow handles it */
    SDL_Quit();
    free(fb);
}
