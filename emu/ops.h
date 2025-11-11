#ifndef OPS_H
#define OPS_H

#include "machine.h"

#define OP(name) void name(Machine* machine, uint32_t op)

typedef struct {
    char* name;
    enum {
        I,
        R,
        RI,
        M,
    } type;
    uint8_t opcode;
    size_t num_operands;
} Opcode;

static Opcode opcodes[64] = {
    { .name = "NOP", .type = R,  .opcode = (uint8_t)0b00000000, .num_operands = 0},
    { .name = "MOV", .type = RI, .opcode = (uint8_t)0b00000100, .num_operands = 2},
    { .name = "HALT",.type = R,  .opcode = (uint8_t)0b01011100, .num_operands = 0},
    { .name = "B",   .type = M,  .opcode = (uint8_t)0b00110100, .num_operands = 1}
};

uint8_t get_opcode_name(uint8_t byte) {
    uint8_t index = 0xFF;
    for (int i = 0; i < 64; i++) {
        if (opcodes[i].opcode == byte) {
            index = i;
            break;
        }
    }
    return index;
}

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
        uint32_t imm = (getbyte(op, 32 - (6 + 4 + 4)) << 8) | getbyte(op, 32 - (6 + 4 + 12));
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

OP(B) {
    int32_t addr = extend((getbyte(op, 32 - 6) << 16) | (getbyte(op, 32 - 6 - 8) << 8) | (getbyte(op, 32 - 6 - 16)));
    machine->cpu.pc = (int32_t)machine->cpu.pc + addr;
}

#endif