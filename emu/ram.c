#include "ram.h"
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 4096
#define WORDS_PER_PAGE (PAGE_SIZE / sizeof(uint32_t))



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
    Page *p = get_page(addr, 0);
    if (!p) return 0; // default value for untouched memory
    uint32_t offset = addr % WORDS_PER_PAGE;
    return p->data[offset];
}
