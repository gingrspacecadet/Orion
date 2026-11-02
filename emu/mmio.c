#include <string.h>
#include <stdlib.h>
#include "mmio.h"

void mmio_rebuild_page_map(Mmio *mmio) {
    memset(mmio->page_map, 0, MMIO_PAGE_COUNT * sizeof(*mmio->page_map));

    for (uint32_t i = 0; i < (uint32_t)mmio->device_count; i++) {
        Device *dev = &mmio->devices[i];

        if (dev->base_addr == 0 || dev->size == 0)
            continue;

        uint64_t end = (uint64_t)dev->base_addr + dev->size;

        if (end > (UINT64_C(1) << 32))
            abort();

        uint32_t first_page = dev->base_addr >> MMIO_PAGE_SHIFT;
        uint32_t last_page = (uint32_t)((end + 4095) >> MMIO_PAGE_SHIFT);

        uint32_t bit = UINT32_C(1) << i;

        for (uint32_t page = first_page; page < last_page; page++)
            mmio->page_map[page] |= bit;
    }
}

static inline void write_u32le(uint8_t *p, size_t offset, uint32_t value) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    value = __builtin_bswap32(value);
#endif
    memcpy(p + offset, &value, sizeof(value));
}

static inline uint32_t read_u32le(const uint8_t *p, size_t offset) {
    uint32_t x;
    memcpy(&x, p + offset, sizeof(x));

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    x = __builtin_bswap32(x);
#endif

    return x;
}

static Device *find_device(Mmio *mmio, uint32_t addr) {
    uint32_t candidates = mmio->page_map[addr >> MMIO_PAGE_SHIFT];

    for (uint32_t i = 0; candidates != 0; i++, candidates >>= 1) {
        if ((candidates & 1) == 0)
            continue;

        Device *dev = &mmio->devices[i];

        if (addr >= dev->base_addr &&
            addr - dev->base_addr < dev->size)
            return dev;
    }

    return NULL;
}

uint32_t mmio_read32(Mmio *mmio, uint32_t addr) {
    Device *dev = find_device(mmio, addr);
    if (dev)
        return dev->read(dev->state, addr - dev->base_addr, sizeof(uint32_t));
    else
        return read_u32le(mmio->mem, addr);
}

void mmio_write32(Mmio *mmio, uint32_t addr, uint32_t value) {
    Device *dev = find_device(mmio, addr);
    if (dev)
        dev->write(dev->state, addr - dev->base_addr, value, sizeof(uint32_t));
    else
        write_u32le(mmio->mem, addr, value);
}

uint8_t mmio_read8(Mmio *mmio, uint32_t addr) {
    Device *dev = find_device(mmio, addr);
    if (dev)
        return dev->read(dev->state, addr - dev->base_addr, sizeof(uint8_t));
    else
        return mmio->mem[addr];
}

void mmio_write8(Mmio *mmio, uint32_t addr, uint8_t value) {
    Device *dev = find_device(mmio, addr);
    if (dev)
        dev->write(dev->state, addr - dev->base_addr, value, sizeof(uint8_t));
    else
       mmio->mem[addr] = value;
}
