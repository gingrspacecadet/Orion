#ifndef PCU_H
#define PCU_H

#include <stdint.h>
#include <stddef.h>
#include "irq.h"
#include "mmio.h"

#define PCU_MAX_DEV_COUNT 16

typedef struct {
    uint32_t id;
    uint32_t base_address;
    uint32_t size;
    IrqLine irq;
    Device *device;
} Descriptor;

typedef struct {
    Descriptor table[PCU_MAX_DEV_COUNT];
    size_t device_count;
} Pcu;

void pcu_add_device(Pcu *pcu, Descriptor device);

uint32_t pcu_read(void *state, uint32_t offset, uint8_t size);
void pcu_write(void *state, uint32_t offset, uint32_t value, uint8_t size);

#define DEVICE_ID(vendor, class, device, revision) \
    (uint32_t)((((uint32_t)(vendor)     & 0xFFFu) << 20) | \
               (((uint32_t)(class)      & 0xFFu)  << 12) | \
               (((uint32_t)(device)     & 0xFFu)  << 4)  | \
               ((uint32_t) (revision)   & 0xFu)   << 0)

#endif