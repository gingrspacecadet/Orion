#ifndef MMIO_H
#define MMIO_H

#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#define MAX_DEVICES 32

typedef uint32_t (*device_read_fn)(void *state, uint32_t offet, uint8_t size);
typedef void     (*device_write_fn)(void *state, uint32_t offset, uint32_t value, uint8_t size);

typedef struct {
    uint32_t base_addr;
    uint32_t size;
    void *state;
    device_read_fn read;
    device_write_fn write;
} Device;

#define MMIO_PAGE_SHIFT 12
#define MMIO_PAGE_COUNT (1u << (32 - MMIO_PAGE_SHIFT))

typedef struct {
    uint8_t *mem;
    Device devices[MAX_DEVICES];
    int device_count;

    uint32_t *page_map;
} Mmio;

static inline void mmio_init(Mmio *mmio) {
    *mmio = (Mmio){
        .mem = mmap(NULL, 1ull << 32, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0),
        .page_map = mmap(NULL, 4ull << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0),
    };
}

uint32_t mmio_read32(Mmio *mmio, uint32_t addr);
void mmio_write32(Mmio *mmio, uint32_t addr, uint32_t value);
uint8_t mmio_read8(Mmio *mmio, uint32_t addr);
void mmio_write8(Mmio *mmio, uint32_t addr, uint8_t value);
void mmio_rebuild_page_map(Mmio *mmio);

static inline void mmio_add_device(Mmio *mmio, Device device) {
    assert(mmio);
    assert(mmio->device_count <= MAX_DEVICES);

    mmio->devices[mmio->device_count++] = device;
    mmio_rebuild_page_map(mmio);
}

#endif