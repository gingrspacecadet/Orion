#ifndef VGAC_H
#define VGAC_H

#include <SDL2/SDL.h>
#include "machine.h"

#define CHAR_W  9
#define CHAR_H  16
#define VGA_W    (80 * CHAR_W)
#define VGA_H    (25 * CHAR_H)

#define VGA_BASE 0xB8000

typedef struct {
    SDL_Window* window;
    SDL_Surface* surface;
    int width;
    int height;
} vga_State;

typedef struct {
    char c;
    char* val;
} Font;

vga_State* vga_init(char* title, size_t width, size_t height);
void vga_render(vga_State* vga, Machine* m, uint32_t addr);
void vga_destroy(vga_State* vga);

#endif