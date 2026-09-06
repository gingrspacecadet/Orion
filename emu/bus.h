#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "mem.h"

#define MAX_DEVICES 32
#define TOTAL_PAGES 1048576

typedef uint32_t (*device_read_fn)(void *state, uint32_t offset, uint8_t size);
typedef void     (*device_write_fn)(void *state, uint32_t offset, uint32_t value, uint8_t size);

typedef struct {
    uint32_t base_addr;
    uint32_t size;
    void *state;
    device_read_fn read;
    device_write_fn write;
} BusDevice;

typedef struct {
    Memory *mem;
    BusDevice devices[MAX_DEVICES];
    int device_count;
    BusDevice *page_map[TOTAL_PAGES];
} Bus;

Bus *bus_init(Memory *mem);
bool bus_register_device(Bus *bus, uint32_t base, uint32_t size, void *state, device_read_fn read, device_write_fn write);
void bus_update_mapping(Bus *bus, int slot, uint32_t new_base);
void bus_rebuild_page_map(Bus *bus);

static inline BusDevice *find_device(Bus *bus, uint32_t addr) {
    for (int i = 0; i < bus->device_count; i++) {
        BusDevice *dev = &bus->devices[i];
        if (dev->base_addr != 0 && addr >= dev->base_addr && addr < dev->base_addr + dev->size) {
            return dev;
        }
    }
    return NULL;
}

static __always_inline uint32_t bus_read32(Bus *bus, uint32_t addr) {
    uint32_t page = addr >> 12;
    BusDevice *dev = bus->page_map[page];
    
    if (__builtin_expect(dev != NULL, 0)) {
        return dev->read(dev->state, addr - dev->base_addr, 4);
    }
    return mem_read32(bus->mem, addr);
}

static __always_inline void bus_write32(Bus *bus, uint32_t addr, uint32_t val) {
    uint32_t page = addr >> 12;
    BusDevice *dev = bus->page_map[page];
    
    if (__builtin_expect(dev != NULL, 0)) {
        dev->write(dev->state, addr - dev->base_addr, val, 4);
        return;
    }
    mem_write32(bus->mem, addr, val);
}

static __always_inline uint8_t bus_read8(Bus *bus, uint32_t addr) {
    uint32_t page = addr >> 12;
    BusDevice *dev = bus->page_map[page];
    
    if (__builtin_expect(dev != NULL, 0)) {
        return dev->read(dev->state, addr - dev->base_addr, 1);
    }
    return mem_read8(bus->mem, addr);
}

static __always_inline void bus_write8(Bus *bus, uint32_t addr, uint8_t val) {
    uint32_t page = addr >> 12;
    BusDevice *dev = bus->page_map[page];
    
    if (__builtin_expect(dev != NULL, 0)) {
        dev->write(dev->state, addr - dev->base_addr, val, 1);
        return;
    }
    mem_write8(bus->mem, addr, val);
}

#endif