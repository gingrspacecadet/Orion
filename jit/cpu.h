#ifndef CPU_H
#define CPU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Cpu {
    uint32_t usr[16];
    uint32_t ksr[16];

    uint32_t *regs;

    uint32_t pc;
    uint32_t flags;

    uint8_t *memory;
    size_t memory_size;
} Cpu;

typedef enum CpuMemResult {
    CPU_MEM_OK,
    CPU_MEM_FAULT,
} CpuMemResult;

CpuMemResult cpu_load32(Cpu *cpu, uint32_t address, uint32_t *value);
CpuMemResult cpu_store32(Cpu *cpu, uint32_t address, uint32_t value);

#endif