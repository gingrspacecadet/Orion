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
