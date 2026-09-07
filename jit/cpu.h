#ifndef CPU_H
#define CPU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Memory Memory;

#define TLB_ENTRIES 32
#define PAGE_SHIFT 12
#define PAGE_SIZE  (1u << PAGE_SHIFT)
#define PAGE_MASK  (PAGE_SIZE - 1)

typedef struct TlbEntry {
    uint32_t hi;
    uint32_t lo;
} TlbEntry;

typedef struct Cpu {
    uint32_t usr[16];
    uint32_t ksr[16];

    uint32_t *regs;

    uint32_t pc;
    uint32_t flags;

    Memory *memory;

    TlbEntry tlb[TLB_ENTRIES];
    uint16_t asid;
} Cpu;

typedef enum CpuMemResult {
    CPU_MEM_OK,
    CPU_MEM_FAULT,
    CPU_MEM_MISALIGNED,
    CPU_MEM_TLB_MISS,
} CpuMemResult;

CpuMemResult cpu_translate(Cpu *cpu, uint32_t vaddr, bool write, bool execute, uint32_t *paddr);

CpuMemResult cpu_load8(Cpu *cpu, uint32_t address, uint32_t *value);
CpuMemResult cpu_load32(Cpu *cpu, uint32_t address, uint32_t *value);
CpuMemResult cpu_store8(Cpu *cpu, uint32_t address, uint32_t value);
CpuMemResult cpu_store32(Cpu *cpu, uint32_t address, uint32_t value);
CpuMemResult cpu_fetch32(Cpu *cpu, uint32_t address, uint32_t *value);

#endif