#ifndef ROM_H
#define ROM_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    size_t size;
} Rom;

Rom *rom_create(size_t size);

#endif