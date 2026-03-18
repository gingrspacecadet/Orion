#include "device.h"
#include "ram.h"

#define MAX_DEVICES 16

typedef struct {
    Device* devices[MAX_DEVICES];
    size_t num;
} DeviceManager;

static DeviceManager mgr;

uint32_t bus_read(uint32_t addr) {
    for (size_t i = 0; i < mgr.num; i++) {
        Device* curr = mgr.devices[i];
        if (addr >= curr->base && addr <= curr->base + curr->size) {
            if (curr->read) 
                return curr->read(curr, addr - curr->base);
        }
    }
    return ram_read(addr);
}

void bus_write(uint32_t addr, uint32_t value) {
    for (size_t i = 0; i < mgr.num; i++) {
        Device* curr = mgr.devices[i];
        if (addr >= curr->base && addr <= curr->base + curr->size) {
            if (curr->write) {
                curr->write(curr, addr - curr->base, value);
                return;
            }
        }
    }
    ram_write(addr, value);
}

void bus_register(Device* dev) {
    if (mgr.num == MAX_DEVICES) return;
    mgr.devices[mgr.num++] = dev;
    if (dev->init) dev->init(dev);
}