#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>

typedef struct Device {
    uint32_t (*read)(struct Device* self, uint32_t addr);
    void (*write)(struct Device* self, uint32_t addr, uint32_t value);
    void (*init)(struct Device* self);
    uint32_t start, end;
} Device;

#endif