#include "machine.h"
#include "../asm/ops.h"

#define OP(name) void name(Machine* machine, uint32_t op)

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
        sign_extend(imm, 16);
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
        sign_extend(imm, 16);
        machine->cpu.registers[reg] = imm;
    }
}

OP(JMP) {
    int32_t addr = sign_extend(get_bits(op, 32 - 6, 32 - 6 - 24), 24);

    int64_t tmp = (int64_t)(int32_t)machine->cpu.pc + (int64_t)addr - 1;
    machine->cpu.pc = (uint32_t)tmp;
}

OP(CALL) {
    machine->memory[machine->cpu.sp--] = machine->cpu.pc;
    JMP(machine, op);
}

OP(RET) {
    machine->cpu.pc = machine->memory[++machine->cpu.sp];
}