#ifndef CPU_H
#define CPU_H

#include <stddef.h>
#include <stdint.h>

typedef struct Memory Memory;

typedef struct Cpu {
    uint32_t usr[16];
    uint32_t ksr[16];

    uint32_t *regs;

    uint32_t pc;
    uint32_t flags;

    Memory *memory;

    uint8_t exception;
} Cpu;

typedef enum CpuMemResult {
    CPU_MEM_OK,
    CPU_MEM_FAULT,
    CPU_MEM_MISALIGNED,
} CpuMemResult;

CpuMemResult cpu_load8(Cpu *cpu, uint32_t address, uint32_t *value);
CpuMemResult cpu_load32(Cpu *cpu, uint32_t address, uint32_t *value);
CpuMemResult cpu_store8(Cpu *cpu, uint32_t address, uint32_t value);
CpuMemResult cpu_store32(Cpu *cpu, uint32_t address, uint32_t value);
CpuMemResult cpu_fetch32(Cpu *cpu, uint32_t address, uint32_t *value);

#endif