#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "machine.h"

#include "fb.h"

static void fb_putpixel_surface(SDL_Surface *surface, int x, int y, uint32_t color) {
    if (!surface) return;
    if (x < 0 || x >= surface->w || y < 0 || y >= surface->h) return;

    int bpp = surface->format->BytesPerPixel;
    uint8_t *pixel = (uint8_t*)surface->pixels + y * surface->pitch + x * bpp;

    switch (bpp) {
        case 1:
            *pixel = (uint8_t)color;
            break;
        case 2:
            *(uint16_t*)pixel = (uint16_t)color;
            break;
        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                pixel[0] = (color >> 16) & 0xFF;
                pixel[1] = (color >> 8) & 0xFF;
                pixel[2] = color & 0xFF;
            } else {
                pixel[0] = color & 0xFF;
                pixel[1] = (color >> 8) & 0xFF;
                pixel[2] = (color >> 16) & 0xFF;
            }
            break;
        case 4:
            *(uint32_t*)pixel = color;
            break;
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
    uint32_t color = SDL_MapRGB(fb->surface->format, r, g, b);
    /* FillRect is fastest for clearing */
    SDL_FillRect(fb->surface, NULL, color);
}

void fb_set_pixel(fb_State* fb, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (!fb || !fb->surface) return;
    uint32_t color = SDL_MapRGB(fb->surface->format, r, g, b);
    fb_putpixel_surface(fb->surface, x, y, color);
}

void fb_render(fb_State* fb, uint32_t** data, size_t width, size_t height) {
    if (!fb || !fb->surface) return;
    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            uint8_t r = getbyte(data[x][y], 32);
            uint8_t g = getbyte(data[x][y], 24);
            uint8_t b = getbyte(data[x][y], 16);
            fb_set_pixel(fb, x, y, r, g, b);
        }
    }
    SDL_UpdateWindowSurface(fb->window);
}

void fb_destroy(fb_State* fb) {
    if (!fb) return;
    if (fb->surface) {
        fb->surface = NULL;
    }
    if (fb->window) {
        SDL_DestroyWindow(fb->window);
    }
    SDL_Quit();
    free(fb);
}
