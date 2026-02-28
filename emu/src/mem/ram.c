#include <ram.h>
#include <xmalloc.h>

Ram *ram_create(size_t size) {
    Ram *ram = xcalloc(sizeof(Ram));
    ram->data = xcalloc(size * sizeof(uint32_t));
    ram->size = size;

    return ram;
}

uint8_t ram_fetch8(Ram *ram, size_t idx) {
    if (!ram) return 0;
    if (idx > ram->size) return 0;
    return ram->data[idx];
}

uint32_t ram_fetch32(Ram *ram, size_t idx) {
    if (!ram) return 0;
    if (idx > ram->size / sizeof(uint32_t)) return 0;

    union { uint32_t word; uint8_t byte[4]; } res;
    size_t act_idx = idx * sizeof(uint32_t);
    res.byte[0] = ram_fetch8(ram, act_idx);
    res.byte[1] = ram_fetch8(ram, act_idx + 1);
    res.byte[2] = ram_fetch8(ram, act_idx + 2);
    res.byte[3] = ram_fetch8(ram, act_idx + 3);

    return res.word;
}