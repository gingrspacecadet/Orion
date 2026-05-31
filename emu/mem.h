#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stdlib.h>

#define PAGE_SIZE   4096
#define NUM_PAGES   (0x100000000ULL / PAGE_SIZE)

typedef struct {
    uint8_t data[PAGE_SIZE];
} MemPage;

typedef struct Memory {
    MemPage **page_table;
} Memory;

static __always_inline MemPage *get_page(Memory *mem, uint32_t addr, bool create) {
    uint32_t page_num = addr / PAGE_SIZE;
    MemPage *p = mem->page_table[page_num];
    if (p || !create) return p;
    
    p = calloc(1, sizeof(MemPage));
    mem->page_table[page_num] = p;
    return p;
}

Memory *mem_init();
void mem_write8(Memory *mem, uint32_t addr, uint8_t v);
uint8_t mem_read8(Memory *mem, uint32_t addr);
uint32_t mem_read32(Memory *mem, uint32_t addr);
void mem_write32(Memory *mem, uint32_t addr, const uint32_t v);

#endif