#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

#define ORION_ZERO_BASE 0x00000000u
#define ORION_ZERO_END  0x00001000u

#define ORION_ROM_BASE  0x00001000u
#define ORION_ROM_END   0x00010000u

#define ORION_IHVT_BASE 0x00010000u
#define ORION_IHVT_END  0x00010400u

#define ORION_ICU_BASE  0x00010400u
#define ORION_ICU_END   0x00010500u

#define ORION_PIT_BASE  0x00010500u
#define ORION_PIT_END   0x00010600u

#define ORION_MTU_BASE  0x00010600u
#define ORION_MTU_END   0x00010800u

#define ORION_PCU_BASE  0x00010800u
#define ORION_PCU_END   0x00020000u

#define ORION_RAM_BASE  0x00020000u

typedef struct Memory {
    uint8_t *rom;
    size_t rom_size;

    uint8_t *ihvt;
    size_t ihvt_size;

    uint8_t *ram;
    size_t ram_size;
} Memory;

void memory_init(Memory *memory, uint8_t *rom, size_t rom_size, uint8_t *ihvt, size_t ihvt_size, uint8_t *ram, size_t ram_size);

#endif