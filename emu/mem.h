#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stdlib.h>

#define PAGE_SIZE   4096
#define NUM_PAGES   (0x100000000ULL / PAGE_SIZE)
#define PAGE_SHIFT 12
#define PAGE_MASK  0xFFF

typedef struct {
    uint8_t data[PAGE_SIZE];
    volatile bool dirty;
} MemPage;

typedef struct Memory {
    MemPage **page_table;
} Memory;

Memory *mem_init();
void mem_write8(Memory *mem, uint32_t addr, uint8_t v);
uint8_t mem_read8(Memory *mem, uint32_t addr);

static __always_inline MemPage *get_page(Memory *mem, uint32_t addr, bool create) {
    uint32_t page_num = addr >> PAGE_SHIFT;
    MemPage *p = mem->page_table[page_num];
    if (__builtin_expect(p != NULL, 1)) return p;
    if (!create) return NULL;
    
    p = calloc(1, sizeof(MemPage));
    mem->page_table[page_num] = p;
    return p;
}

static __always_inline uint32_t mem_read32(Memory *mem, uint32_t addr) {
    uint32_t page_num = addr >> PAGE_SHIFT;
    uint32_t offset = addr & PAGE_MASK;

    if (__builtin_expect(offset <= PAGE_SIZE - 4, 1)) {
        MemPage *p = mem->page_table[page_num];
        if (__builtin_expect(p == NULL, 0)) return 0;
        
        uint32_t v;
        memcpy(&v, &p->data[offset], sizeof(v));
        return v;
    }

    return ((uint32_t)mem_read8(mem, addr)) |
           ((uint32_t)mem_read8(mem, addr + 1) << 8) |
           ((uint32_t)mem_read8(mem, addr + 2) << 16) |
           ((uint32_t)mem_read8(mem, addr + 3) << 24);
}

static __always_inline void mem_write32(Memory *mem, uint32_t addr, const uint32_t v) {
    uint32_t page_num = addr >> PAGE_SHIFT;
    uint32_t offset = addr & PAGE_MASK;

    if (__builtin_expect(offset <= PAGE_SIZE - 4, 1)) {
        MemPage *p = mem->page_table[page_num];
        
        if (__builtin_expect(p == NULL, 0)) {
            p = get_page(mem, addr, true);
        }
        
        memcpy(&p->data[offset], &v, sizeof(v));
        p->dirty = true;
        return;
    }

    mem_write8(mem, addr, (uint8_t)v);
    mem_write8(mem, addr + 1, (uint8_t)(v >> 8));
    mem_write8(mem, addr + 2, (uint8_t)(v >> 16));
    mem_write8(mem, addr + 3, (uint8_t)(v >> 24));
}

#endif