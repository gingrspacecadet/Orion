#include "machine.h"
#include "../asm/ops.h"
#include "fb.h"

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

OP(HLT) {
    (void)op;
    machine->cpu.running = false;
}

OP(ADD) {
    bool I_type = (getbit(op, 0) != 0);
    uint8_t dest = getbits(op, 25, 22);
    uint8_t src1 = getbits(op, 21, 18);

    if (!I_type) { // R-type
        uint8_t src2 = getbits(op, 17, 14);
        machine->cpu.registers[dest] = machine->cpu.registers[src1] + machine->cpu.registers[src2];
    } else { // I-type
        int32_t imm = sign_extend(getbits(op, 17, 2), 16);

        machine->cpu.registers[dest] = machine->cpu.registers[src1] + (uint32_t)imm;
    }
}

OP(JMP) {
    int32_t addr = sign_extend(getbits(op, 32 - 6, 32 - 6 - 24), 24);

    int64_t tmp = (int64_t)(int32_t)machine->cpu.pc + (int64_t)addr - 1;
    machine->cpu.pc = (uint32_t)tmp;
}

#define push(value) (machine->ram[machine->cpu.sp--] = value)
#define pop(dest) (dest = machine->ram[++machine->cpu.sp])

OP(CALL) {
    push(machine->cpu.pc);
    JMP(machine, op);
}

OP(RET) {
    (void)op;
    pop(machine->cpu.pc);
}

OP(INT) {
    if (machine->mode == BIOS) {
        machine->mode = KERNEL;
        machine->cpu.pc = 0;
    } else {
        int32_t imm = sign_extend(getbits(op, 17, 2), 16);
        machine->cpu.pc = machine->ram[0x1234 + imm];
        machine->mode = BIOS;
    }
}

OP(LDR) {
    uint8_t dest    = (uint8_t)getbits(op, 25, 22);
    uint8_t base    = (uint8_t)getbits(op, 21, 18);
    int32_t imm     = sign_extend((int32_t)getbits(op, 17, 2), 16);

    uint32_t addr_index = (uint32_t)((int32_t)machine->cpu.registers[base] + imm);

    machine->cpu.registers[dest] = machine->ram[addr_index];
}

OP(STR) {
    uint8_t src_reg = (uint8_t)getbits(op, 25, 22); /* register containing the value to store */
    uint8_t base    = (uint8_t)getbits(op, 21, 18);
    int32_t imm     = sign_extend((int32_t)getbits(op, 17, 2), 16);

    uint32_t addr_index = (uint32_t)((int32_t)machine->cpu.registers[base] + imm);

    machine->ram[addr_index] = machine->cpu.registers[src_reg];

    extern fb_State* fb;
    fb_render(fb, &machine->ram[FB_BASE]);
}

OP(PUSH) {
    uint16_t mask     = (uint16_t)getbits(op, 17, 2);
    for (size_t i = 0; i < sizeof(uint16_t) * 2; i++) {
        if (getbit(mask, i) == 1) {
            push(machine->cpu.registers[i]);
        }
    }
}

OP(POP) {
    uint16_t mask     = (uint16_t)getbits(op, 17, 2);
    for (size_t i = 0; i < sizeof(uint16_t) * 2; i++) {
        if (getbit(mask, i) == 1) {
            pop(machine->cpu.registers[i]);
        }
    }
}

OP(SHL) {
    bool I_type = (getbit(op, 0) != 0);
    uint8_t dest = getbits(op, 25, 22);
    uint8_t src1 = getbits(op, 21, 18);

    if (!I_type) { // R-type
        uint8_t src2 = getbits(op, 17, 14);
        machine->cpu.registers[dest] = machine->cpu.registers[src1] << (machine->cpu.registers[src2] & 31);
    } else { // I-type
        uint32_t imm = getbits(op, 17, 2);

        machine->cpu.registers[dest] = machine->cpu.registers[src1] << (imm & 31);
    }
}

OP(OR) {
    bool I_type = (getbit(op, 0) != 0);
    uint8_t dest = getbits(op, 25, 22);
    uint8_t src1 = getbits(op, 21, 18);

    if (!I_type) { // R-type
        uint8_t src2 = getbits(op, 17, 14);
        machine->cpu.registers[dest] = machine->cpu.registers[src1] | machine->cpu.registers[src2];
    } else { // I-type
        int32_t imm = getbits(op, 17, 2);

        machine->cpu.registers[dest] = machine->cpu.registers[src1] | (uint32_t)imm;
    }
}