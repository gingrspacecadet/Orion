#ifndef RAM_H
#define RAM_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    size_t size;
} Ram;

Ram *ram_create(size_t size);
uint8_t ram_fetch8(Ram *ram, size_t idx);
uint32_t ram_fetch32(Ram *ram, size_t idx);

#endif