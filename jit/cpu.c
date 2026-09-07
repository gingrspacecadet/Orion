#include "cpu.h"
#include "memory.h"

static CpuMemResult memory_read8(Cpu *cpu, uint32_t address, uint32_t *value) {
    Memory *memory = cpu->memory;

    if (address < ORION_ZERO_END)
        return CPU_MEM_FAULT;

    if (address >= ORION_ROM_BASE && address < ORION_ROM_END) {
        uint32_t offset = address - ORION_ROM_BASE;

        if ((uint64_t)offset >= memory->rom_size)
            return CPU_MEM_FAULT;

        *value = memory->rom[offset];
        return CPU_MEM_OK;
    }

    if (address >= ORION_IHVT_BASE && address < ORION_IHVT_END) {
        uint32_t offset = address - ORION_IHVT_BASE;

        if ((uint64_t)offset >= memory->ihvt_size)
            return CPU_MEM_FAULT;

        *value = memory->ihvt[offset];
        return CPU_MEM_OK;
    }

    if (address >= ORION_RAM_BASE) {
        uint32_t offset = address - ORION_RAM_BASE;

        if ((uint64_t)offset >= memory->ram_size)
            return CPU_MEM_FAULT;

        *value = memory->ram[offset];
        return CPU_MEM_OK;
    }

    return CPU_MEM_FAULT;
}

static CpuMemResult memory_read32(Cpu *cpu, uint32_t address, uint32_t *value) {
    if (address & 3)
        return CPU_MEM_MISALIGNED;

    uint32_t b0;
    uint32_t b1;
    uint32_t b2;
    uint32_t b3;

    if (memory_read8(cpu, address + 0, &b0) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    if (memory_read8(cpu, address + 1, &b1) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    if (memory_read8(cpu, address + 2, &b2) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    if (memory_read8(cpu, address + 3, &b3) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    *value = b0
           | b1 << 8
           | b2 << 16
           | b3 << 24;

    return CPU_MEM_OK;
}

static CpuMemResult memory_write8(Cpu *cpu, uint32_t address, uint32_t value) {
    Memory *memory = cpu->memory;

    if (address < ORION_ZERO_END)
        return CPU_MEM_FAULT;

    if (address >= ORION_ROM_BASE && address < ORION_ROM_END)
        return CPU_MEM_FAULT;

    if (address >= ORION_IHVT_BASE && address < ORION_IHVT_END)
        return CPU_MEM_OK;

    if (address >= ORION_RAM_BASE) {
        uint32_t offset = address - ORION_RAM_BASE;

        if ((uint64_t)offset >= memory->ram_size)
            return CPU_MEM_FAULT;

        memory->ram[offset] = (uint8_t)value;
        return CPU_MEM_OK;
    }

    return CPU_MEM_FAULT;
}

static CpuMemResult memory_write32(Cpu *cpu, uint32_t address, uint32_t value) {
    if (address & 3)
        return CPU_MEM_MISALIGNED;

    if (memory_write8(cpu, address + 0, value >> 0) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    if (memory_write8(cpu, address + 1, value >> 8) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    if (memory_write8(cpu, address + 2, value >> 16) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    if (memory_write8(cpu, address + 3, value >> 24) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    return CPU_MEM_OK;
}

CpuMemResult cpu_load8(Cpu *cpu, uint32_t address, uint32_t *value) {
    return memory_read8(cpu, address, value);
}

CpuMemResult cpu_load32(Cpu *cpu, uint32_t address, uint32_t *value) {
    return memory_read32(cpu, address, value);
}

CpuMemResult cpu_store8(Cpu *cpu, uint32_t address, uint32_t value) {
    return memory_write8(cpu, address, value);
}

CpuMemResult cpu_store32(Cpu *cpu, uint32_t address, uint32_t value) {
    return memory_write32(cpu, address, value);
}