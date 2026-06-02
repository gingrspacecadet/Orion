#include "gpu.h"
#include <signal.h>
#include <stdio.h>

static void gpu_timer_callback(void *state) {
    GpuDevice *gpu = (GpuDevice *)state;
    gpu_refresh_frame(gpu);
    timer_mod(gpu->timer_pool, &gpu->vblank_timer, gpu->timer_pool->virtual_time + 16666667);
}

GpuDevice *gpu_create(Bus *bus, TimerPool *pool, IrqLine irq) {
    GpuDevice *gpu = calloc(1, sizeof(GpuDevice));
    if (!gpu) return NULL;

    gpu->bus = bus;
    gpu->timer_pool = pool;
    gpu->irq = irq;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "GPU Error: SDL init failed: %s\n", SDL_GetError());
        free(gpu);
        return NULL;
    }

    gpu->window = SDL_CreateWindow("Orion Display",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   640, 480, SDL_WINDOW_SHOWN);
    if (!gpu->window) { gpu_destroy(gpu); return NULL; }

    gpu->renderer = SDL_CreateRenderer(gpu->window, -1, SDL_RENDERER_ACCELERATED);
    if (!gpu->renderer) { gpu_destroy(gpu); return NULL; }

    gpu->texture = SDL_CreateTexture(gpu->renderer, SDL_PIXELFORMAT_RGB565, 
                                     SDL_TEXTUREACCESS_STREAMING, 320, 240);
    if (!gpu->texture) { gpu_destroy(gpu); return NULL; }

    timer_init(&gpu->vblank_timer, gpu_timer_callback, gpu);
    timer_mod(pool, &gpu->vblank_timer, pool->virtual_time + 16666667);

    return gpu;
}

void gpu_destroy(GpuDevice *gpu) {
    if (!gpu) return;
    if (gpu->timer_pool && gpu->vblank_timer.active) {
        timer_del(gpu->timer_pool, &gpu->vblank_timer);
    }
    if (gpu->texture) SDL_DestroyTexture(gpu->texture);
    if (gpu->renderer) SDL_DestroyRenderer(gpu->renderer);
    if (gpu->window) SDL_DestroyWindow(gpu->window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    free(gpu);
}

uint32_t gpu_bus_read(void *state, uint32_t offset, uint8_t size) {
    GpuDevice *gpu = (GpuDevice *)state;
    switch (offset & 0xF) {
        case 0x00: return gpu->fb_base_addr;
        case 0x04: return gpu->vblank_status;
        case 0x08: return gpu->int_config;
        case 0x0C: return gpu->video_mode;
        default:   return 0;
    }
}

void gpu_bus_write(void *state, uint32_t offset, uint32_t value, uint8_t size) {
    GpuDevice *gpu = (GpuDevice *)state;
    switch (offset & 0xF) {
        case 0x00: gpu->fb_base_addr = value; break;
        case 0x04: break; 
        case 0x08: gpu->int_config = value; break;
        case 0x0C: gpu->video_mode = value; break;
    }
}

void gpu_refresh_frame(GpuDevice *gpu) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            raise(SIGINT);
            return;
        }
    }

    if (gpu->video_mode != 1 || gpu->fb_base_addr == 0) {
        SDL_SetRenderDrawColor(gpu->renderer, 0, 0, 0, 255);
        SDL_RenderClear(gpu->renderer);
        SDL_RenderPresent(gpu->renderer);
        return;
    }
    
    gpu->vblank_status = 1; 
    
    if (gpu->int_config & 1) {
        irq_raise(gpu->irq);
    }
    
    bool system_changed = false;
    uint32_t total_bytes = 320 * 240 * sizeof(uint16_t);
    uint32_t active_pages = (total_bytes + (PAGE_SIZE - 1)) / PAGE_SIZE; 
    
    for (uint32_t i = 0; i < active_pages; i++) {
        uint32_t guest_address = gpu->fb_base_addr + (i * PAGE_SIZE);
        MemPage *page = get_page(gpu->bus->mem, guest_address, false);
        if (page && page->dirty) {
            page->dirty = false; 
            system_changed = true;
            
            uint32_t frame_offset = i * PAGE_SIZE;
            uint32_t segment_len = PAGE_SIZE;
            if (frame_offset + segment_len > total_bytes) {
                segment_len = total_bytes - frame_offset;
            }
            
            uint8_t *dest_ptr = ((uint8_t *)gpu->host_back_buffer) + frame_offset;
            memcpy(dest_ptr, page->data, segment_len);
        }
    }
    
    if (system_changed) {
        SDL_UpdateTexture(gpu->texture, NULL, gpu->host_back_buffer, 320 * sizeof(uint16_t));
        SDL_RenderClear(gpu->renderer);
        SDL_RenderCopy(gpu->renderer, gpu->texture, NULL, NULL);
        SDL_RenderPresent(gpu->renderer);
    }
    
    gpu->vblank_status = 0; 
    irq_lower(gpu->irq);
}