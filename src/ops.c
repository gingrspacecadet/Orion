#include "ops.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#define fetch16(m) ((uint16_t)(fetch8(m) | (fetch8(m) << 8)))
#define mem_write_ok(m, addr, len) ((addr) + (len) <= MEM_SIZE)
#define mem_read_ok(m, addr, len) ((addr) + (len) <= MEM_SIZE)
#define push8(m, v) \
    ( (m->cpu.sp < 1) ? (fprintf(stderr, "Stack overflow\n"), m->cpu.running = false, (void)0) : \
    (void)(m->cpu.sp -= sizeof(uint8_t), m->mem[m->cpu.sp] = (v)) )

#define pop8(m) \
    ( (m->cpu.sp >= MEM_SIZE) ? (fprintf(stderr, "Stack underflow\n"), m->cpu.running = false, (uint8_t)0) : \
      ( { uint8_t _v = m->mem[m->cpu.sp]; m->cpu.sp += sizeof(uint8_t); _v; } ) )
    
#define push16(m, v) \
    ( (m->cpu.sp < 2) ? (fprintf(stderr, "Stack overflow\n"), m->cpu.running = false, (void)0) : \
    (void)(m->cpu.sp -= sizeof(uint16_t), m->mem[m->cpu.sp    ] = (uint8_t)((v) & 0xFF), \
    m->mem[m->cpu.sp + 1] = (uint8_t)(((v) >> 8) & 0xFF)) )

#define pop16(m) \
    ( (m->cpu.sp + 1 >= MEM_SIZE) ? (fprintf(stderr, "Stack underflow\n"), m->cpu.running = false, (uint16_t)0) : \
      ( { uint16_t _lo = m->mem[m->cpu.sp]; uint16_t _hi = m->mem[m->cpu.sp + 1]; m->cpu.sp += sizeof(uint16_t); (uint16_t)((_hi << 8) | _lo); } ) )

OP(LDI) {
    uint8_t reg_byte = fetch8(m);
    uint8_t lit = fetch8(m);
    uint8_t idx = reg_byte & 7;
    m->cpu.reg[idx] = lit;
}

OP(HLT) {
    m->cpu.running = false;
}

OP(ADD) {
    uint8_t r1b = fetch8(m);
    uint8_t r2b = fetch8(m);
    uint8_t r1 = r1b & 7;
    uint8_t r2 = r2b & 7;
    uint16_t result = (uint16_t)getreg(m, r1) + (uint16_t)getreg(m, r2);
    m->cpu.reg[r1] = (uint8_t)result;
    m->cpu.flags.Z = (m->cpu.reg[r1] == 0);
}

OP(SUB) {
    uint8_t r1b = fetch8(m);
    uint8_t r2b = fetch8(m);
    uint8_t r1 = r1b & 7;
    uint8_t r2 = r2b & 7;
    int16_t result = (int16_t)getreg(m, r1) - (int16_t)getreg(m, r2);
    m->cpu.reg[r1] = (uint8_t)result;
    m->cpu.flags.Z = (m->cpu.reg[r1] == 0);
}

OP(MUL) {
    uint8_t r1b = fetch8(m);
    uint8_t r2b = fetch8(m);
    uint8_t r1 = r1b & 7;
    uint8_t r2 = r2b & 7;
    uint16_t result = (uint16_t)getreg(m, r1) * (uint16_t)getreg(m, r2);
    m->cpu.reg[r1] = (uint8_t)result;
    m->cpu.flags.Z = (m->cpu.reg[r1] == 0);
}

OP(DIV) {
    uint8_t r1b = fetch8(m);
    uint8_t r2b = fetch8(m);
    uint8_t r1 = r1b & 7;
    uint8_t r2 = r2b & 7;
    uint8_t rhs = getreg(m, r2);
    if (rhs == 0) { fprintf(stderr, "Division by zero\n"); m->cpu.running = false; return; }
    m->cpu.reg[r1] = (uint8_t)(getreg(m, r1) / rhs);
    m->cpu.flags.Z = (m->cpu.reg[r1] == 0);
}

OP(MOD) {
    uint8_t r1b = fetch8(m);
    uint8_t r2b = fetch8(m);
    uint8_t r1 = r1b & 7;
    uint8_t r2 = r2b & 7;
    uint8_t rhs = getreg(m, r2);
    if (rhs == 0) { fprintf(stderr, "Modulo by zero\n"); m->cpu.running = false; return; }
    m->cpu.reg[r1] = (uint8_t)(getreg(m, r1) % rhs);
    m->cpu.flags.Z = (m->cpu.reg[r1] == 0);
}

OP(CMP) {
    uint8_t r1b = fetch8(m);
    uint8_t r2b = fetch8(m);
    uint8_t r1 = r1b & 7;
    uint8_t r2 = r2b & 7;
    int16_t diff = (int16_t)getreg(m, r1) - (int16_t)getreg(m, r2);
    m->cpu.flags.Z = (diff == 0);
}

OP(JZ) {
    uint16_t target = fetch8(m);
    if (m->cpu.flags.Z) {
        m->cpu.pc = target;
    }
}

OP(JNZ) {
    uint16_t target = fetch8(m);
    if (!m->cpu.flags.Z) {
        m->cpu.pc = target;
    }
}

OP(JMP) {
    uint16_t target = fetch8(m);
    m->cpu.pc = target;
}

OP(PUSH) {
    uint8_t regb = fetch8(m);
    uint8_t r = regb & 7;
    push8(m, (uint8_t)getreg(m, r));
}

OP(POP) {
    uint8_t regb = fetch8(m);
    uint8_t r = regb & 7;
    m->cpu.reg[r] = pop8(m);
}

OP(CALL) {
    /* push return address (current PC) as 16-bit, then jump to target */
    uint16_t target = fetch8(m);
    push16(m, m->cpu.pc);
    m->cpu.pc = target;
}

OP(RET) {
    /* pop return address (16-bit) */
    uint16_t ret = pop16(m);
    m->cpu.pc = ret;
}

OP(MOV) {
    uint8_t r1b = fetch8(m);
    uint8_t r2b = fetch8(m);
    uint8_t r1 = r1b & 7;
    uint8_t r2 = r2b & 7;
    m->cpu.reg[r1] = m->cpu.reg[r2];
}

OP(IRET) {
    /* restore PC (16-bit) and re-enable interrupts if desired.
       Using 16-bit pop to match push16 used in step() earlier. */
    uint16_t ret = pop16(m);
    m->cpu.pc = ret;
    /* optionally re-enable interrupt flag here if your ABI wants that */
}

OP(NOP) {
    return;
}

OP(STI) {
    m->cpu.flags.IE = true;
}

OP(CLI) {
    m->cpu.flags.IE = false;
}

OP(LDR) {
    uint8_t regb = fetch8(m);
    uint8_t addr_byte = fetch8(m);
    uint8_t r = regb & 7;
    uint16_t addr = (uint16_t)addr_byte;
    if (!mem_read_ok(m, addr, 1)) { fprintf(stderr, "LDR address out of bounds\n"); m->cpu.running = false; return; }
    m->cpu.reg[r] = m->mem[addr];
}

OP(STR) {
    uint8_t addr_byte = fetch8(m);
    uint8_t regb = fetch8(m);
    uint8_t r = regb & 7;
    uint16_t addr = (uint16_t)addr_byte;
    if (!mem_write_ok(m, addr, 1)) { fprintf(stderr, "STR address out of bounds\n"); m->cpu.running = false; return; }
    m->mem[addr] = (uint8_t)getreg(m, r);
}
