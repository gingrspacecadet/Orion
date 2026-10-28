#include <string.h>
#include "mmio.h"

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
    assert(mmio);

    for (int i = 0; i < mmio->device_count; i++) {
        Device *dev = &mmio->devices[i];

        if (dev->base_addr != 0 && addr >= dev->base_addr && addr < dev->base_addr + dev->size) {
            return dev;
        }
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
