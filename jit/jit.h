#ifndef JIT_H
#define JIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cpu.h"
#include "isa.h"

typedef enum JitExit {
    JIT_EXIT_NEXT,
    JIT_EXIT_FAULT,
} JitExit;

typedef JitExit (*JitFn)(Cpu *cpu);
typedef CpuMemResult (*JitFetch)(Cpu *cpu, uint32_t pc, uint32_t *value);

typedef struct JitPage {
    uint8_t *code;
    size_t size;
} JitPage;

typedef struct JitBlock {
    uint32_t pc;
    JitFn fn;
} JitBlock;

typedef struct Jit {
    JitPage *pages;
    size_t page_count;
    size_t page_cap;
    size_t page_size;

    JitBlock *blocks;
    size_t block_count;
    size_t block_cap;

    JitFetch fetch;
} Jit;

bool jit_init(Jit *jit, size_t page_cap, size_t block_cap, JitFetch fetch);
void jit_destroy(Jit *jit);
JitBlock *jit_get_block(Jit *jit, Cpu *cpu, uint32_t pc);

#endif