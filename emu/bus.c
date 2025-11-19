#include "device.h"
#include "bus.h"

#define MAX_DEVICES 16

typedef struct {
    Device* devices[MAX_DEVICES];
    size_t num;
} DeviceManager;

static DeviceManager mgr;

uint32_t bus_read(Machine* m, uint32_t addr) {
    for (size_t i = 0; i < mgr.num; i++) {
        if (addr >= mgr.devices[i]->start && addr <= mgr.devices[i]->end) {
            return mgr.devices[i]->read(mgr.devices[i], addr);
        }
    }
    return m->ram[addr];
}

void bus_write(Machine* m, uint32_t addr, uint32_t value) {
    for (size_t i = 0; i < mgr.num; i++) {
        if (addr >= mgr.devices[i]->start && addr <= mgr.devices[i]->end) {
            mgr.devices[i]->write(mgr.devices[i], addr, value);
            return;
        }
    }
    m->ram[addr] = value;
}