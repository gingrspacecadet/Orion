#include "memory.h"

void memory_init(Memory *memory, uint8_t *rom, size_t rom_size, uint8_t *ihvt, size_t ihvt_size, uint8_t *ram, size_t ram_size) {
    memory->rom = rom;
    memory->rom_size = rom_size;
    memory->ihvt = ihvt;
    memory->ihvt_size = ihvt_size;
    memory->ram = ram;
    memory->ram_size = ram_size;
}