#include "device.h"

#define MAX_DEVICES 16

typedef struct {
    Device* devices[MAX_DEVICES];
    size_t num;
} DeviceManager;

static DeviceManager mgr;

uint32_t bus_read(Machine* m, uint32_t addr) {
    for (size_t i = 0; i < mgr.num; i++) {
        if (addr >= mgr.devices[i]->base && addr <= mgr.devices[i]->base + mgr.devices[i]->size) {
            return mgr.devices[i]->read(mgr.devices[i], addr - mgr.devices[i]->base);
        }
    }
    return m->ram[addr];
}

void bus_write(Machine* m, uint32_t addr, uint32_t value) {
    for (size_t i = 0; i < mgr.num; i++) {
        if (addr >= mgr.devices[i]->base && addr <= mgr.devices[i]->base + mgr.devices[i]->size) {
            mgr.devices[i]->write(mgr.devices[i], addr, value);
            return;
        }
    }
    m->ram[addr] = value;
}

void bus_register(Machine* m, Device* dev) {
    if (mgr.num == MAX_DEVICES) return;
    mgr.devices[mgr.num++] = dev;
}