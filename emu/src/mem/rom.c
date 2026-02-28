#include <rom.h>
#include <xmalloc.h>
#include <stddef.h>
#include <stdint.h>

Rom *rom_create(size_t size) {
    Rom *rom = xcalloc(sizeof(Rom));
    rom->data = xcalloc(size * sizeof(uint32_t));
    rom->size = size;

    return rom;
}