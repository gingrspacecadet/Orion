#include <stdlib.h>
#include "bus.h"

Bus *bus_init(Memory *mem) {
    Bus *bus = calloc(1, sizeof(Bus));
    bus->mem = mem;
    return bus;
}

bool bus_register_device(Bus *bus, uint32_t size, void *state, device_read_cb read, device_write_cb write) {
    if (bus->device_count >= MAX_DEVICES) return false;

    BusDevice *dev = &bus->devices[bus->device_count++];
    dev->base_addr = 0;
    dev->size = size;
    dev->state = state;
    dev->read = read;
    dev->write = write;

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
    }
}

static BusDevice* find_device(Bus *bus, uint32_t addr) {
    for (int i = 0; i < bus->device_count; i++) {
        BusDevice *dev = &bus->devices[i];

        if (dev->base_addr == 0) continue;

        // Clean, foolproof range check
        if (addr >= dev->base_addr && addr < (dev->base_addr + dev->size)) {
            return dev;
        }
    }
    return NULL;
}

uint8_t bus_read8(Bus *bus, uint32_t addr) {
    BusDevice *dev = find_device(bus, addr);
    if (dev) return dev->read(dev->state, addr - dev->base_addr, 1);
    else return mem_read8(bus->mem, addr);
}

uint32_t bus_read32(Bus *bus, uint32_t addr) {
    BusDevice *dev = find_device(bus, addr);
    if (dev) return dev->read(dev->state, addr - dev->base_addr, 4);
    else return mem_read32(bus->mem, addr);
}

void bus_write8(Bus *bus, uint32_t addr, uint8_t val) {
    BusDevice *dev = find_device(bus, addr);
    if (dev) {
        dev->write(dev->state, addr - dev->base_addr, val, 1);
        return;
    }
    mem_write8(bus->mem, addr, val);
}

void bus_write32(Bus *bus, uint32_t addr, uint32_t val) {
    BusDevice *dev = find_device(bus, addr);
    if (dev) {
        dev->write(dev->state, addr - dev->base_addr, val, 4);
        return;
    }
    mem_write32(bus->mem, addr, val);
}