#include "memory.h"

void memory_init(Memory *memory, uint8_t *ram, size_t ram_size) {
    memory->ram = ram;
    memory->ram_size = ram_size;
}