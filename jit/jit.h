#ifndef JIT_H
#define JIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "isa.h"

typedef uint32_t (*JitFn)(uint32_t *regs);
typedef uint32_t (*JitFetch)(void *ctx, uint32_t pc);

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
    void *fetch_ctx;
} Jit;

bool jit_init(Jit *jit, size_t page_cap, size_t block_cap, JitFetch fetch, void *fetch_ctx);
void jit_destroy(Jit *jit);
JitBlock *jit_get_block(Jit *jit, uint32_t pc);

#endif