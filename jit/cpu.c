#include "cpu.h"
#include "memory.h"

#define ORION_ZERO_PAGE_END 0x00001000u
#define ORION_RAM_BASE      0x00020000u

CpuMemResult cpu_load8(Cpu *cpu, uint32_t address, uint32_t *value) {
    if (address < ORION_ZERO_PAGE_END)
        return CPU_MEM_FAULT;

    if (address < ORION_RAM_BASE)
        return CPU_MEM_FAULT;

    uint32_t offset = address - ORION_RAM_BASE;

    if ((uint64_t)offset >= cpu->memory->ram_size)
        return CPU_MEM_FAULT;

    *value = cpu->memory->ram[offset];

    return CPU_MEM_OK;
}

CpuMemResult cpu_load32(Cpu *cpu, uint32_t address, uint32_t *value) {
    if (address & 3)
        return CPU_MEM_MISALIGNED;

    if (address < ORION_ZERO_PAGE_END)
        return CPU_MEM_FAULT;

    if (address < ORION_RAM_BASE)
        return CPU_MEM_FAULT;

    uint32_t offset = address - ORION_RAM_BASE;

    if ((uint64_t)offset + 4 > cpu->memory->ram_size)
        return CPU_MEM_FAULT;

    *value = (uint32_t)cpu->memory->ram[offset + 0]
           | (uint32_t)cpu->memory->ram[offset + 1] << 8
           | (uint32_t)cpu->memory->ram[offset + 2] << 16
           | (uint32_t)cpu->memory->ram[offset + 3] << 24;

    return CPU_MEM_OK;
}

CpuMemResult cpu_store8(Cpu *cpu, uint32_t address, uint32_t value) {
    if (address < ORION_ZERO_PAGE_END)
        return CPU_MEM_FAULT;

    if (address < ORION_RAM_BASE)
        return CPU_MEM_FAULT;

    uint32_t offset = address - ORION_RAM_BASE;

    if ((uint64_t)offset >= cpu->memory->ram_size)
        return CPU_MEM_FAULT;

    cpu->memory->ram[offset] = (uint8_t)value;

    return CPU_MEM_OK;
}

CpuMemResult cpu_store32(Cpu *cpu, uint32_t address, uint32_t value) {
    if (address & 3)
        return CPU_MEM_MISALIGNED;

    if (address < ORION_ZERO_PAGE_END)
        return CPU_MEM_FAULT;

    if (address < ORION_RAM_BASE)
        return CPU_MEM_FAULT;

    uint32_t offset = address - ORION_RAM_BASE;

    if ((uint64_t)offset + 4 > cpu->memory->ram_size)
        return CPU_MEM_FAULT;

    cpu->memory->ram[offset + 0] = (uint8_t)(value >> 0);
    cpu->memory->ram[offset + 1] = (uint8_t)(value >> 8);
    cpu->memory->ram[offset + 2] = (uint8_t)(value >> 16);
    cpu->memory->ram[offset + 3] = (uint8_t)(value >> 24);

    return CPU_MEM_OK;
}