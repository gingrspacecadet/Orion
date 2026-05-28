#ifndef PCU_H
#define PCU_H

#include "bus.h"

#define PCU_MAX_DEVICES 16

typedef struct {
    uint32_t device_id;
    uint32_t base_addr;
    uint32_t component_size;
    uint32_t reserved;
} PcuSlot;

typedef struct {
    Bus *bus; // reference back to the bus to modify BARs dynamically
    PcuSlot slots[PCU_MAX_DEVICES];
} PcuDevice;

uint32_t pcu_internal_read(void *state, uint32_t offset, uint8_t size);

void pcu_internal_write(void *state, uint32_t offset, uint32_t value, uint8_t size);

#define MAKE_DEVICE_ID(vendor, cls, device, rev) \
    (uint32_t)((((uint32_t)(vendor) & 0xFFFu) << 20) | \
               (((uint32_t)(cls)    & 0xFFu)   << 12)  | \
               (((uint32_t)(device) & 0xFFu)  << 4) | \
               ((uint32_t)(rev)    & 0xFu))

#endif