#ifndef RAM_H
#define RAM_H

#include <stdint.h>
#include "machine.h"

typedef struct Page {
    uint32_t data[WORDS_PER_PAGE];
} Page;

typedef struct Node {
    uint32_t page_num;
    Page *page;
    struct Node *next;
} Node;

Node* page_table = NULL;

void ram_init(void);
void ram_free(void);
void ram_write(uint32_t addr, uint32_t value);
uint32_t ram_read(uint32_t addr);

#endif
