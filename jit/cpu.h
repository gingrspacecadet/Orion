#ifndef CPU_H
#define CPU_H

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

#endif