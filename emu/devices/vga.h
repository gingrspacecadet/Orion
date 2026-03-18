#ifndef VGAC_H
#define VGAC_H

#include <SDL2/SDL.h>
#include "machine.h"
#include "../device.h"

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
    uint32_t* mem;
    uint16_t addr;
} VGA;

typedef struct {
    char c;
    char* val;
} Font;

void vga_init(Device* self);
void vga_render(VGA* vga);
void vga_destroy(VGA* vga);

extern VGA vga_state;

extern Device vga_device;

#endif