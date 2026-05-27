#include "mem.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#define PAGE_SIZE   4096

typedef struct {
    uint8_t data[PAGE_SIZE];
} MemPage;

typedef struct MemNode {
    uint32_t page_num;
    MemPage *page;
    struct MemNode *next;
} MemNode;

typedef struct Memory {
    MemNode *page_table;
} Memory;

static MemPage *get_page(Memory *mem, uint32_t addr, bool create) {
    uint32_t page_num = addr / PAGE_SIZE;
    MemNode *n = mem->page_table;
    while (n) {
        if (n->page_num == page_num) return n->page;
        n = n->next;
    }
    
    if (!create) return NULL;

    MemPage *p = calloc(1, sizeof(MemPage));
    MemNode *new_node = malloc(sizeof(MemNode));
    new_node->page_num = page_num;
    new_node->page = p;
    new_node->next = mem->page_table;
    mem->page_table = new_node;
    return p;
}

Memory *mem_init() {
    Memory *mem = calloc(1, sizeof(Memory));
    mem->page_table = NULL;
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

uint32_t mem_read32(Memory *mem, uint32_t addr) {
    return ((uint32_t)mem_read8(mem, addr)) |
           ((uint32_t)mem_read8(mem, addr + 1) << 8) |
           ((uint32_t)mem_read8(mem, addr + 2) << 16) |
           ((uint32_t)mem_read8(mem, addr + 3) << 24);
}

void mem_write32(Memory *mem, uint32_t addr, const uint32_t v) {
    mem_write8(mem, addr, v);
    mem_write8(mem, addr + 1, v >> 8);
    mem_write8(mem, addr + 2, v >> 16);
    mem_write8(mem, addr + 3, v >> 24);
}