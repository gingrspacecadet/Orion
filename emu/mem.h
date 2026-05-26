#ifndef MEM_H
#define MEM_H

#include <stdint.h>

typedef struct Memory Memory;

Memory *mem_init();
void mem_write8(Memory *mem, uint32_t addr, uint8_t v);
uint8_t mem_read8(Memory *mem, uint32_t addr);
uint32_t mem_read32(Memory *mem, uint32_t addr);
void mem_write32(Memory *mem, uint32_t addr, const uint32_t v);

#endif