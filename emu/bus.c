#include <stdlib.h>
#include "bus.h"

Bus *bus_init(Memory *mem) {
    Bus *bus = calloc(1, sizeof(Bus));
    bus->mem = mem;
    return bus;
}

void bus_rebuild_page_map(Bus *bus) {
    memset(bus->page_map, 0, sizeof(bus->page_map));
    
    for (int i = 0; i < bus->device_count; i++) {
        BusDevice *dev = &bus->devices[i];
        
        if (dev->size == 0 || dev->base_addr == 0) continue;

        uint32_t start_page = dev->base_addr >> 12;
        uint64_t end_addr = (uint64_t)dev->base_addr + dev->size;
        uint32_t end_page = (end_addr + 4095) >> 12;
        
        if (end_page > TOTAL_PAGES) end_page = TOTAL_PAGES;

        for (uint32_t p = start_page; p < end_page; p++) {
            bus->page_map[p] = dev;
        }
    }
}

bool bus_register_device(Bus *bus, uint32_t base, uint32_t size, void *state, device_read_fn read, device_write_fn write) {
    if (bus->device_count >= MAX_DEVICES) return false;

    BusDevice *dev = &bus->devices[bus->device_count++];
    dev->base_addr = 0;
    dev->size = size;
    dev->state = state;
    dev->read = read;
    dev->write = write;

    bus_update_mapping(bus, bus->device_count - 1, base);
    return true;
}

void bus_update_mapping(Bus *bus, int slot, uint32_t new_base) {
    if (slot >= 0 && slot < bus->device_count) {
        BusDevice *dev = &bus->devices[slot];
        if (dev->size > 0) {
            dev->base_addr = (new_base / dev->size) * dev->size;
        } else {
            dev->base_addr = new_base;
        }
        bus_rebuild_page_map(bus);
    }
}