#ifndef MACHINE_H
#define MACHINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint32_t pc;
    uint32_t sp;
    uint32_t registers[16];
    uint8_t flags;
    bool running;
} CPU;

typedef struct {
    CPU cpu;
    uint32_t ram[0xFFFF];
    uint32_t rom[0xFFFF];
    enum {
        BIOS,
        KERNEL,
        USER,
    } mode;
} Machine;

#define fetch(machine) (machine->mode != BIOS ? machine->ram[machine->cpu.pc++] : machine->rom[machine->cpu.pc++])
#define getbit(target, bit) ((target & (1 << bit)) >> bit)

static inline uint8_t getbyte(uint32_t target, uint16_t start) {
    uint8_t _val = 0;
    for (int i = 0; i < start; i++) {
        _val |= getbit(target, (start - i)) << (8 - i);
    };
    return _val;
}

static inline uint32_t getbits(uint32_t v, int hi, int lo) {
    uint32_t mask = ((1u << (hi - lo + 1)) - 1u);
    return (v >> lo) & mask;
}


static inline int32_t sign_extend(uint32_t value, unsigned bits) {
    if (bits == 0 || bits >= 32) return (int32_t)value;
    uint32_t shift = 32 - bits;
    return (int32_t)(value << shift) >> shift;
}


#define FLAG_ZERO 0
#define FLAG_CARRY 1
#define FLAG_NEGATIVE 2
#define FLAG_OVERFLOW 3

#endif