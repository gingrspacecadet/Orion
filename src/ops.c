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