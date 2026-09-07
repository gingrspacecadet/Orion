#include "cpu.h"
#include "memory.h"
#include "isa.h"

#define FLAG_PRV (1u << 1)
#define FLAG_VM  (1u << 2)

#define TLB_HI_VALID 0x1u
#define TLB_HI_ASID_MASK 0x7FEu

#define TLB_LO_W  (1u << 3)
#define TLB_LO_U  (1u << 2)
#define TLB_LO_C  (1u << 1)
#define TLB_LO_WI (1u << 0)

#define FLAG_IE  (1u << 0)
#define FLAG_PRV (1u << 1)
#define FLAG_VM  (1u << 2)
#define FLAG_TM  (1u << 3)

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

    *value = b0 |
             b1 << 8 |
             b2 << 16 |
             b3 << 24;

    return CPU_MEM_OK;
}

static CpuMemResult memory_write8(Cpu *cpu, uint32_t address, uint32_t value) {
    Memory *memory = cpu->memory;

    if (address < ORION_ZERO_END)
        return CPU_MEM_FAULT;

    if (address >= ORION_ROM_BASE && address < ORION_ROM_END)
        return CPU_MEM_FAULT;

    if (address >= ORION_IHVT_BASE && address < ORION_IHVT_END)
        return CPU_MEM_FAULT;

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

    if (memory_write8(cpu, address + 0, value & 0xFF) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    if (memory_write8(cpu, address + 1, (value >> 8) & 0xFF) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    if (memory_write8(cpu, address + 2, (value >> 16) & 0xFF) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    if (memory_write8(cpu, address + 3, (value >> 24) & 0xFF) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    return CPU_MEM_OK;
}

CpuMemResult cpu_translate(Cpu *cpu, uint32_t vaddr, bool write, bool execute, uint32_t *paddr) {
    if ((cpu->flags & FLAG_VM) == 0) {
        *paddr = vaddr;
        return CPU_MEM_OK;
    }

    uint32_t vpn = vaddr >> PAGE_SHIFT;
    uint32_t offset = vaddr & PAGE_MASK;

    for (size_t i = 0; i < TLB_ENTRIES; i++) {
        TlbEntry *entry = &cpu->tlb[i];

        if ((entry->hi & TLB_HI_VALID) == 0)
            continue;

        uint32_t entry_vpn = entry->hi >> PAGE_SHIFT;
        uint16_t entry_asid = (entry->hi & TLB_HI_ASID_MASK) >> 1;

        if (entry_vpn != vpn)
            continue;

        if (entry_asid != cpu->asid)
            continue;

        if (!write && !execute && (cpu->flags & FLAG_PRV) == 0 && (entry->lo & TLB_LO_U) == 0)
            return CPU_MEM_FAULT;

        if (write && (entry->lo & TLB_LO_W) == 0)
            return CPU_MEM_FAULT;

        uint32_t pfn = entry->lo >> PAGE_SHIFT;
        *paddr = (pfn << PAGE_SHIFT) | offset;
        return CPU_MEM_OK;
    }

    return CPU_MEM_TLB_MISS;
}

CpuMemResult cpu_load8(Cpu *cpu, uint32_t address, uint32_t *value) {
    uint32_t physical;

    CpuMemResult result = cpu_translate(cpu, address, false, false, &physical);
    if (result != CPU_MEM_OK)
        return result;

    return memory_read8(cpu, physical, value);
}

CpuMemResult cpu_load32(Cpu *cpu, uint32_t address, uint32_t *value) {
    if (address & 3)
        return CPU_MEM_MISALIGNED;

    uint32_t physical;

    CpuMemResult result = cpu_translate(cpu, address, false, false, &physical);
    if (result != CPU_MEM_OK)
        return result;

    return memory_read32(cpu, physical, value);
}

CpuMemResult cpu_store8(Cpu *cpu, uint32_t address, uint32_t value) {
    uint32_t physical;

    CpuMemResult result = cpu_translate(cpu, address, true, false, &physical);
    if (result != CPU_MEM_OK)
        return result;

    return memory_write8(cpu, physical, value);
}

CpuMemResult cpu_store32(Cpu *cpu, uint32_t address, uint32_t value) {
    if (address & 3)
        return CPU_MEM_MISALIGNED;

    uint32_t physical;

    CpuMemResult result = cpu_translate(cpu, address, true, false, &physical);
    if (result != CPU_MEM_OK)
        return result;

    return memory_write32(cpu, physical, value);
}

CpuMemResult cpu_fetch32(Cpu *cpu, uint32_t address, uint32_t *value) {
    if (address & 3)
        return CPU_MEM_MISALIGNED;

    uint32_t physical;

    CpuMemResult result = cpu_translate(cpu, address, false, true, &physical);
    if (result != CPU_MEM_OK)
        return result;

    return memory_read32(cpu, physical, value);
}

CpuMemResult cpu_raise_exception(Cpu *cpu, uint8_t vector) {
    uint32_t *old_regs = cpu->regs;
    uint32_t old_sp = old_regs[15];

    cpu->flags |= FLAG_PRV;
    cpu->flags &= ~FLAG_IE;

    cpu->regs = cpu->ksr;

    uint32_t sp = cpu->regs[15];

    if (sp < 8)
        return CPU_MEM_FAULT;

    sp -= 4;

    if (memory_write32(cpu, sp, cpu->pc) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    sp -= 4;

    if (memory_write32(cpu, sp, cpu->flags) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    cpu->regs[15] = sp;

    uint32_t handler;

    if (memory_read32(cpu, IHVT_BASE + (uint32_t)vector * 4, &handler) != CPU_MEM_OK)
        return CPU_MEM_FAULT;

    cpu->pc = handler;

    (void)old_sp;

    return CPU_MEM_OK;
}