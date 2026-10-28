#include <assert.h>
#include "pcu.h"
#include "isa.h"

void pcu_add_device(Pcu *pcu, Descriptor device) {
    assert(pcu->device_count < PCU_MAX_DEV_COUNT); assert(device.device); assert(device.base_address == device.device->base_addr); assert((device.size & (device.size - 1)) == 0); assert(device.size != 0); assert(device.irq.pin == UINT32_MAX || (device.irq.pin >= 1 && device.irq.pin < 32));
    pcu->table[pcu->device_count++] = device;
}

uint32_t pcu_read(void *state, uint32_t offset, uint8_t size) {
    assert(state);

    Pcu *pcu = state;

    if (size != 4)
        return 0;

    Descriptor device = pcu->table[offset / sizeof(Descriptor)];

    switch (offset % sizeof(Descriptor)) {
        case PCU_DEV_ID: return device.id;
        case PCU_DEV_BAR: return device.base_address;
        case PCU_DEV_SIZE: return device.size;
        case PCU_DEV_IRQ: return device.irq.pin;
        default: break;
    }

    return 0;
}


void pcu_write(void *state, uint32_t offset, uint32_t value, uint8_t size) {
    assert(state);

    Pcu *pcu = state;

    if (size != 4)
        return;

    Descriptor *device = &pcu->table[offset / sizeof(Descriptor)];

    switch (offset % sizeof(Descriptor)) {
        case PCU_DEV_BAR: {
            if (value % device->size != 0) return;      // not aligned
            if (value + device->size < value) return;   // wraps address space
            device->base_address = value;
            device->device->base_addr = value;
            return;
        }

        default: break;
    }

    return;
}