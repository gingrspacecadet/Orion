#ifndef FB_H
#define FB_H

#include <SDL2/SDL.h>

typedef struct {
    SDL_Window* window;
    SDL_Surface* surface;
    int width;
    int height;
} fb_State;

fb_State* fb_init(char* title, size_t width, size_t height);
void fb_render(fb_State* fb, uint32_t** data, size_t width, size_t height);
void fb_destroy(fb_State* fb);

#endif