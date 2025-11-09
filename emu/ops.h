#ifndef OPS_H
#define OPS_H

#include "machine.h"

#define OP(name) void name(Machine* machine, uint32_t op)

OP(NOP) {
    (void)machine; (void)op;
    return;
}

OP(MOV) {
    if (getbit(op, 0) == 0) { // R-type
        uint8_t reg1 = getbyte(op, 32 - 6);
        uint8_t reg2 = reg1 ^ ((reg1 >> 4) << 4);
        reg1 = reg1 >> 4;
        machine->cpu.registers[reg1] = machine->cpu.registers[reg2];
    } else { // I-type
        uint8_t reg = getbyte(op, 32 - 6) >> 4;
        uint32_t imm = (getbyte(op, 32 - (6 + 4 + 4)) << 8) | getbyte(op, 32 - (6 + 4 + 8));
        extend(imm);
        machine->cpu.registers[reg] = imm;
    }
}

OP(HALT) {
    (void)op;
    machine->cpu.running = false;
}

OP(ADD) {
    if (getbit(op, 0) == 0) { // R-type
        uint8_t reg1 = getbyte(op, 32 - 6);
        uint8_t reg2 = reg1 ^ ((reg1 >> 4) << 4);
        uint8_t reg3 = getbyte(op, 32 - 6 - 8) >> 4;
        reg1 = reg1 >> 4;
        machine->cpu.registers[reg1] = machine->cpu.registers[reg2] + machine->cpu.registers[reg3];
    } else { // I-type
        uint8_t reg = getbyte(op, 32 - 6) >> 4;
        uint32_t imm = (getbyte(op, 32 - (6 + 4 + 4)) << 8) | getbyte(op, 32 - (6 + 4 + 8));
        extend(imm);
        machine->cpu.registers[reg] = imm;
    }
}

#endif