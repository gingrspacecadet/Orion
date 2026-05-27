#include "pcu.h"
#include <stdio.h>

uint32_t pcu_internal_read(void *state, uint32_t offset, uint8_t size) {
    PcuDevice *pcu = (PcuDevice *)state;
    int slot = offset / PCU_SLOT_SIZE;
    int reg_offset = offset % PCU_SLOT_SIZE;

    if (slot >= PCU_MAX_DEVICES) return 0;
    PcuSlotRegisters *regs = &pcu->slots[slot];

    switch (reg_offset) {
        case 0x00:  return regs->device_id;
        case 0x04:  return regs->base_addr;
        case 0x08:  return regs->component_size;
        case 0x0C:  return regs->reserved;
        default:    return 0;
    }
}

void pcu_internal_write(void *state, uint32_t offset, uint32_t value, uint8_t size) {
    PcuDevice *pcu = (PcuDevice *)state;
    int slot = offset / PCU_SLOT_SIZE;
    int reg_offset = offset % PCU_SLOT_SIZE;

    if (slot >= PCU_MAX_DEVICES) return;
    PcuSlotRegisters *regs = &pcu->slots[slot];

    if (reg_offset == 0x04) {
        regs->base_addr = value;
        bus_update_mapping(pcu->bus, slot, value);
    } else if (reg_offset == 0x0C) {
        regs->reserved = value;
    }

    // TODO: throw EX_INVALID_MEM when writing to RO section
}