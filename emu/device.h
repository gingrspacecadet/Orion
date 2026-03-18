#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include "machine.h"

typedef struct Device {
    uint32_t (*read)(struct Device* self, uint32_t addr);
    void (*write)(struct Device* self, uint32_t addr, uint32_t value);
    void (*init)(struct Device* self);
    uint32_t base;
    size_t size;
    void* state;
} Device;

uint32_t bus_read(uint32_t addr);
void bus_write(uint32_t addr, uint32_t value);
void bus_register(Device* dev);

#endif