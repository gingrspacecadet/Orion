#include "cpu.h"

#include "cpu.h"

CpuMemResult cpu_load32(Cpu *cpu, uint32_t address, uint32_t *value) {
    if ((address & 3) != 0)
        return CPU_MEM_FAULT;

    if ((uint64_t)address + 4 > cpu->memory_size)
        return CPU_MEM_FAULT;

    *value = (uint32_t)cpu->memory[address + 0]
           | (uint32_t)cpu->memory[address + 1] << 8
           | (uint32_t)cpu->memory[address + 2] << 16
           | (uint32_t)cpu->memory[address + 3] << 24;

    return CPU_MEM_OK;
}

CpuMemResult cpu_store32(Cpu *cpu, uint32_t address, uint32_t value) {
    if ((address & 3) != 0)
        return CPU_MEM_FAULT;

    if ((uint64_t)address + 4 > cpu->memory_size)
        return CPU_MEM_FAULT;

    cpu->memory[address + 0] = (uint8_t)(value >> 0);
    cpu->memory[address + 1] = (uint8_t)(value >> 8);
    cpu->memory[address + 2] = (uint8_t)(value >> 16);
    cpu->memory[address + 3] = (uint8_t)(value >> 24);

    return CPU_MEM_OK;
}