#ifndef MACHINE_H
#define MACHINE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t pc;
    uint32_t sp;
    uint32_t registers[16];
    uint8_t flags;
    bool running;
} CPU;

typedef struct {
    CPU cpu;
    uint32_t memory[0xFFFF];
} Machine;

#define getbit(target, bit) ((target & (1 << bit)) >> bit)

static inline uint8_t getbyte(uint32_t target, uint16_t start) {
    uint8_t _val = 0;
    for (int i = 0; i < start; i++) {
        _val |= getbit(target, (start - i)) << (8 - i);
    };
    return _val;
}

#define fetch(machine) (machine->memory[machine->cpu.pc++])

static inline uint32_t extend(uint16_t target) {
    if (getbit(target, (sizeof((target) * 2) - 1)) == 1) { 
        for (size_t i = 0; i < (31 - (sizeof((target) * 2) - 1)); i++) { 
            target |= 1 << i; 
        }
    }
    return target;
}


#define FLAG_ZERO 0
#define FLAG_CARRY 1
#define FLAG_NEGATIVE 2
#define FLAG_OVERFLOW 3

#endif