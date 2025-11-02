#ifndef MACHINE_H
#define MACHINE_H

#include <stdint.h>
#include <stdbool.h>

#define fetch8(machine) machine->mem[machine->cpu.pc++]
#define OP(name) void name(Machine* m)
#define getreg(machine, register) machine->cpu.reg[register & 7]

#define MEM_SIZE 0xFFFF

typedef struct {
    uint8_t reg[8];
    uint16_t pc;
    uint16_t sp;
    uint16_t interrupt;
    struct {
        uint8_t Z:1;
        uint8_t N:1;
        uint8_t C:1;
        uint8_t V:1;
        uint8_t IE:1;
    } flags;
    bool running;
} CPU;

typedef struct {
    CPU cpu;
    uint8_t mem[MEM_SIZE]; // 64kb
} Machine;

#endif