#ifndef GPU_H
#define GPU_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "timer.h"
#include "bus.h"
#include "irq.h"

typedef struct {
    uint32_t fb_base_addr;
    uint32_t vblank_status;
    uint32_t int_config;
    uint32_t video_mode;

    Bus *bus;
    IrqLine irq;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    uint16_t host_back_buffer[320 * 240];

    Timer vblank_timer;
    TimerPool *timer_pool;
} GpuDevice;

GpuDevice *gpu_create(Bus *bus, TimerPool *pool, IrqLine irq);
void gpu_destroy(GpuDevice *gpu);
void gpu_refresh_frame(GpuDevice *gpu);

uint32_t gpu_bus_read(void *state, uint32_t offset, uint8_t size);
void gpu_bus_write(void *state, uint32_t offset, uint32_t value, uint8_t size);

#endif