#ifndef FBC_H
#define FBC_H

#include <SDL2/SDL.h>

#define CHAR_W  9
#define CHAR_H  16
#define FB_W    (80 * CHAR_W)
#define FB_H    (25 * CHAR_H)

#define FB_BASE 0xB8000

typedef struct {
    SDL_Window* window;
    SDL_Surface* surface;
    int width;
    int height;
} fb_State;

typedef struct {
    char c;
    char* val;
} Font;

fb_State* fb_init(char* title, size_t width, size_t height);
void fb_render(fb_State* fb, uint32_t* data);
void fb_destroy(fb_State* fb);

#endif