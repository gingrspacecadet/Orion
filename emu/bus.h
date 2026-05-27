#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include <stdbool.h>
#include "mem.h"

#define MAX_DEVICES 32

typedef uint32_t (*device_read_cb)(void *state, uint32_t offset, uint8_t size);
typedef void     (*device_write_cb)(void *state, uint32_t offset, uint32_t value, uint8_t size);

typedef struct {
    uint32_t base_addr;
    uint32_t size;
    void *state;
    device_read_cb read;
    device_write_cb write;
} BusDevice;

typedef struct {
    Memory *mem;
    BusDevice devices[MAX_DEVICES];
    int device_count;
} Bus;

Bus *bus_init(Memory *mem);
bool bus_register_device(Bus *bus, uint32_t base, uint32_t size, void *state, device_read_cb read, device_write_cb write);

uint8_t bus_read8(Bus *bus, uint32_t addr);
uint32_t bus_read32(Bus *bus, uint32_t addr);
void bus_write8(Bus *bus, uint32_t addr, uint8_t val);
void bus_write32(Bus *bus, uint32_t addr, uint32_t val);

#endif