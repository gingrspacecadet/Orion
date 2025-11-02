#include "ops.h"
#include <stdbool.h>

OP(LDI) {
    uint8_t reg = fetch8(m);
    uint8_t lit = fetch8(m);
    m->cpu.reg[reg & 7] = lit;
}

OP(HLT) {
    m->cpu.running = false;
}

OP(ADD) {
    uint8_t reg1 = fetch8(m);
    uint8_t reg2 = fetch8(m);
    m->cpu.reg[reg1 & 7] = getreg(m, reg1) + getreg(m, reg2);
}

OP(SUB) {
    uint8_t reg1 = fetch8(m);
    uint8_t reg2 = fetch8(m);
    m->cpu.reg[reg1 & 7] = getreg(m, reg1) - getreg(m, reg2);
}

OP(MUL) {
    uint8_t reg1 = fetch8(m);
    uint8_t reg2 = fetch8(m);
    m->cpu.reg[reg1 & 7] = getreg(m, reg1) * getreg(m, reg2);
}

OP(DIV) {
    uint8_t reg1 = fetch8(m);
    uint8_t reg2 = fetch8(m);
    m->cpu.reg[reg1 & 7] = getreg(m, reg1) / getreg(m, reg2);
}

OP(MOD) {
    uint8_t reg1 = fetch8(m);
    uint8_t reg2 = fetch8(m);
    m->cpu.reg[reg1 & 7] = getreg(m, reg1) % getreg(m, reg2);
}

OP(CMP) {
    uint8_t reg1 = fetch8(m);
    uint8_t reg2 = fetch8(m);
    int8_t result = getreg(m, reg1) - getreg(m, reg2);
    m->cpu.flags.Z = result == 0;
}

OP(JZ) {
    if (m->cpu.flags.Z == true) {
        m->cpu.pc = fetch8(m);
    }
}

OP(JNZ) {
    if (m->cpu.flags.Z != true) {
        m->cpu.pc = fetch8(m);
    }
}

OP(JMP) {
    m->cpu.pc = fetch8(m);
}

OP(PUSH) {
    m->cpu.sp -= sizeof(uint8_t);
    m->mem[m->cpu.sp] = m->cpu.reg[fetch8(m) & 7];
}

OP(POP) {
    m->cpu.reg[fetch8(m) & 7] = m->mem[m->cpu.sp];
    m->cpu.sp += sizeof(uint8_t);
}

OP(CALL) {

}

OP(RET) {

}

OP(MOV) {
    uint8_t reg1 = fetch8(m);
    uint8_t reg2 = fetch8(m);
    m->cpu.reg[reg1 & 7] = m->cpu.reg[reg2 & 7];
}

OP(IRET) {
    m->cpu.pc = m->mem[m->cpu.sp];
    m->cpu.sp += sizeof(uint8_t);
}

OP(NOP) {
    return;
}