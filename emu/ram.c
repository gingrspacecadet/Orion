#include "ram.h"
#include <stdlib.h>
#include <string.h>
#include "machine.h"

Node* page_table = NULL;
extern Machine* global_machine;

static Page* get_page(uint32_t addr, int create) {
    uint32_t page_num = addr / WORDS_PER_PAGE;
    Node *n = page_table;
    while (n) {
        if (n->page_num == page_num) return n->page;
        n = n->next;
    }
    if (!create) return NULL;

    // allocate new page lazily
    Page *p = calloc(1, sizeof(Page));
    Node *new_node = malloc(sizeof(Node));
    new_node->page_num = page_num;
    new_node->page = p;
    new_node->next = page_table;
    page_table = new_node;
    return p;
}

void ram_init(void) {
    page_table = NULL;
}

void ram_free(void) {
    Node *n = page_table;
    while (n) {
        Node *next = n->next;
        free(n->page);
        free(n);
        n = next;
    }
    page_table = NULL;
}

void ram_write(uint32_t addr, uint32_t value) {
    Page *p = get_page(addr, 1);
    uint32_t offset = addr % WORDS_PER_PAGE;
    p->data[offset] = value;
}

uint32_t ram_read(uint32_t addr) {
    uint32_t page_num = addr / WORDS_PER_PAGE;
    uint32_t cached_page_num = global_machine->cpu.cache.addr / WORDS_PER_PAGE;

    Page *p;

    if (global_machine->cpu.cache.data && cached_page_num == page_num) {
        p = global_machine->cpu.cache.data;
    } else {
        p = get_page(addr, 0);
        global_machine->cpu.cache.data = p;
        global_machine->cpu.cache.addr = page_num * WORDS_PER_PAGE;
    }

    if (!p) return 0; // default value for untouched memory
    uint32_t offset = addr % WORDS_PER_PAGE;
    return p->data[offset];
}
