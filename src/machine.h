#ifndef MACHINE_H
#define MACHINE_H

#include <stdint.h>

#define fetch8(machine) machine->mem[machine->cpu.pc++];
#define OP(name) void name(Machine* m)
#define getreg(machine, register) machine->cpu.reg[register & 7]

typedef struct {
    uint8_t reg[8];
    uint16_t pc;
    struct {
        uint8_t Z:1;
        uint8_t N:1;
        uint8_t C:1;
        uint8_t V:1;
    } flag;
    bool running;
} CPU;

typedef struct {
    CPU cpu;
    uint8_t mem[0x10000]; // 64kb
} Machine;


#endif