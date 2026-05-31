#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "mem.h"

Memory *mem_init() {
    Memory *mem = calloc(1, sizeof(Memory));
    mem->page_table = calloc(NUM_PAGES, sizeof(MemPage *));
    return mem;
}

void mem_write8(Memory *mem, uint32_t addr, uint8_t v) {
    MemPage *p = get_page(mem, addr, true);
    uint32_t offset = addr % PAGE_SIZE;
    p->data[offset] = v;
}

uint8_t mem_read8(Memory *mem, uint32_t addr) {
    MemPage *p = get_page(mem, addr, false);

    if (!p) return 0;
    uint32_t offset = addr % PAGE_SIZE;
    return p->data[offset];
}

static inline int host_is_little_endian_runtime(void) {
    uint16_t x = 1;
    return (*(uint8_t *)&x) == 1;
}

uint32_t mem_read32(Memory *mem, uint32_t addr) {
    uint32_t offset = addr % PAGE_SIZE;

    if (offset <= PAGE_SIZE - 4) {
        MemPage *p = get_page(mem, addr, false);
        if (!p) return 0;
        uint32_t v;
        memcpy(&v, &p->data[offset], sizeof(v));
        return v;
    }

    return ((uint32_t)mem_read8(mem, addr)) |
           ((uint32_t)mem_read8(mem, addr + 1) << 8) |
           ((uint32_t)mem_read8(mem, addr + 2) << 16) |
           ((uint32_t)mem_read8(mem, addr + 3) << 24);
}

void mem_write32(Memory *mem, uint32_t addr, const uint32_t v) {
    uint32_t offset = addr % PAGE_SIZE;

    if (offset <= PAGE_SIZE - 4) {
        MemPage *p = get_page(mem, addr, true);
        memcpy(&p->data[offset], &v, sizeof(v));
        return;
    }

    mem_write8(mem, addr, v);
    mem_write8(mem, addr + 1, v >> 8);
    mem_write8(mem, addr + 2, v >> 16);
    mem_write8(mem, addr + 3, v >> 24);
}