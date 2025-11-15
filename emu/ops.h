#include "machine.h"
#include "../asm/ops.h"
#include "fb.h"

#define OP(name) void name(Machine* m, uint32_t op)

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

void push(Machine* m, uint32_t value) {
    if (m->cpu.sp == 0) {
        printf("Stack overflow!");
        exit(1);
    }
    m->ram[m->cpu.sp--] = value;
}

uint32_t pop(Machine* m) {
    if (m->cpu.sp == RAM_SIZE) {
        printf("Stack underflow!");
        exit(1);
    }
    return m->ram[++m->cpu.sp];
}

OP(NOP) {
    (void)m; (void)op;
    return;
}

OP(MOV) {
    if (getbit(op, 0) == 0) { // R-type
        uint8_t reg1 = getbyte(op, 32 - 6);
        uint8_t reg2 = reg1 ^ ((reg1 >> 4) << 4);
        reg1 = reg1 >> 4;
        m->cpu.registers[reg1] = m->cpu.registers[reg2];
    } else { // I-type
        uint8_t reg = getbyte(op, 32 - 6) >> 4;
        uint32_t imm = (getbyte(op, 32 - (6 + 4 + 4)) << 8) | getbyte(op, 32 - (6 + 4 + 12));
        sign_extend(imm, 16);
        m->cpu.registers[reg] = imm;
    }
}

OP(HLT) {
    (void)op;
    m->cpu.running = false;
}

OP(ADD) {
    bool I_type = (getbit(op, 0) != 0);
    uint8_t dest = getbits(op, 25, 22);
    uint8_t src1 = getbits(op, 21, 18);

    if (!I_type) { // R-type
        uint8_t src2 = getbits(op, 17, 14);
        m->cpu.registers[dest] = m->cpu.registers[src1] + m->cpu.registers[src2];
    } else { // I-type
        int32_t imm = sign_extend(getbits(op, 17, 2), 16);

        m->cpu.registers[dest] = m->cpu.registers[src1] + (uint32_t)imm;
    }
}

OP(JMP) {
    int32_t addr = sign_extend(getbits(op, 32 - 6, 32 - 6 - 24), 24);

    int64_t tmp = (int64_t)(int32_t)m->cpu.pc + (int64_t)addr - 1;
    m->cpu.pc = (uint32_t)tmp;
}

OP(CALL) {
    push(m, m->cpu.pc);
    JMP(m, op);
}

OP(RET) {
    (void)op;
    m->cpu.pc = pop(m);
}

OP(INT) {
    if (m->mode == BIOS) {
        m->mode = KERNEL;
        m->cpu.pc = 0;
    } else {
        int32_t imm = sign_extend(getbits(op, 17, 2), 16);
        push(m, m->cpu.pc);
        m->cpu.pc = m->ram[0x1234 + imm];
        m->mode = BIOS;
    }
}

OP(LDR) {
    uint8_t dest    = (uint8_t)getbits(op, 25, 22);
    uint8_t base    = (uint8_t)getbits(op, 21, 18);
    int32_t imm     = sign_extend((int32_t)getbits(op, 17, 2), 16);

    uint32_t addr_index = (uint32_t)((int32_t)m->cpu.registers[base] + imm);

    m->cpu.registers[dest] = m->ram[addr_index];
}

OP(STR) {
    uint8_t src_reg = (uint8_t)getbits(op, 25, 22); /* register containing the value to store */
    uint8_t base    = (uint8_t)getbits(op, 21, 18);
    int32_t imm     = sign_extend((int32_t)getbits(op, 17, 2), 16);

    uint32_t addr_index = (uint32_t)((int32_t)m->cpu.registers[base] + imm);

    m->ram[addr_index] = m->cpu.registers[src_reg];

    extern fb_State* fb;
    fb_render(fb, &m->ram[FB_BASE]);
}

OP(PUSH) {
    uint16_t mask = (uint16_t)getbits(op, 17, 2);
    for (int i = 0; i < 16; ++i) {
        if (getbit(mask, i)) {
            push(m, m->cpu.registers[i]);
        }
    }
}

OP(POP) {
    uint16_t mask = (uint16_t)getbits(op, 17, 2);
    for (int i = 15; i >= 0; --i) {
        if (getbit(mask, i)) {
            m->cpu.registers[i] = pop(m);
        }
    }
}

OP(SHL) {
    bool I_type = (getbit(op, 0) != 0);
    uint8_t dest = getbits(op, 25, 22);
    uint8_t src1 = getbits(op, 21, 18);

    if (!I_type) { // R-type
        uint8_t src2 = getbits(op, 17, 14);
        m->cpu.registers[dest] = m->cpu.registers[src1] << (m->cpu.registers[src2] & 31);
    } else { // I-type
        uint32_t imm = getbits(op, 17, 2);

        m->cpu.registers[dest] = m->cpu.registers[src1] << (imm & 31);
    }
}

OP(OR) {
    bool I_type = (getbit(op, 0) != 0);
    uint8_t dest = getbits(op, 25, 22);
    uint8_t src1 = getbits(op, 21, 18);

    if (!I_type) { // R-type
        uint8_t src2 = getbits(op, 17, 14);
        m->cpu.registers[dest] = m->cpu.registers[src1] | m->cpu.registers[src2];
    } else { // I-type
        int32_t imm = getbits(op, 17, 2);

        m->cpu.registers[dest] = m->cpu.registers[src1] | (uint32_t)imm;
    }
}

OP(IRET) {
    m->mode = ~m->mode;
    m->cpu.pc = pop(m);
}