#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

typedef struct Memory {
    uint8_t *ram;
    size_t ram_size;
} Memory;

void memory_init(Memory *memory, uint8_t *ram, size_t ram_size);

#endif