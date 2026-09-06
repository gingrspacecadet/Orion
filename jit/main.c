#include <stdint.h>
#include <stdio.h>

#include "isa.h"
#include "jit.h"

#define MEMORY_SIZE 0x2000
#define PROGRAM_PC 0x1000

static uint32_t fetch(void *ctx, uint32_t pc) {
    uint8_t *memory = ctx;

    return (uint32_t)memory[pc + 0]
         | (uint32_t)memory[pc + 1] << 8
         | (uint32_t)memory[pc + 2] << 16
         | (uint32_t)memory[pc + 3] << 24;
}

static uint32_t encode_reg(Opcode opcode, uint8_t rd, uint8_t rn, uint8_t rm) {
    return ((uint32_t)opcode << 26)
         | ((uint32_t)rn << 22)
         | ((uint32_t)rd << 18)
         | ((uint32_t)rm << 2)
         | (1u << IS_REG_SHIFT);
}

static uint32_t encode_jump(Opcode opcode, uint32_t target) {
    return ((uint32_t)opcode << 26) | (target & 0x03FFFFFF);
}

static void write32(uint8_t *memory, uint32_t address, uint32_t value) {
    memory[address + 0] = (uint8_t)(value >> 0);
    memory[address + 1] = (uint8_t)(value >> 8);
    memory[address + 2] = (uint8_t)(value >> 16);
    memory[address + 3] = (uint8_t)(value >> 24);
}

int main(void) {
    uint8_t memory[MEMORY_SIZE] = {0};
    uint32_t regs[16] = {0};

    regs[1] = 100;
    regs[2] = 23;
    regs[4] = 10;
    regs[6] = 0xFF;

    write32(memory, PROGRAM_PC + 0, encode_reg(OP_ADD, 0, 1, 2));
    write32(memory, PROGRAM_PC + 4, encode_reg(OP_SUB, 3, 0, 4));
    write32(memory, PROGRAM_PC + 8, encode_reg(OP_XOR, 5, 3, 6));
    write32(memory, PROGRAM_PC + 12, encode_reg(OP_ADD, 7, 5, 0));
    write32(memory, PROGRAM_PC + 16, encode_jump(OP_JMP, 0xF0));

    Jit jit;

    if (!jit_init(&jit, 16, fetch, memory))
        return 1;

    JitFn fn = jit_compile(&jit, PROGRAM_PC);

    if (fn == NULL)
        return 1;

    uint32_t pc = fn(regs);

    printf("r0 = %u\n", regs[0]);
    printf("r3 = %u\n", regs[3]);
    printf("r5 = %u\n", regs[5]);
    printf("r7 = %u\n", regs[7]);
    printf("pc = 0x%08X\n", pc);

    jit_destroy(&jit);

    return pc == PROGRAM_PC + 16
        && regs[0] == 123
        && regs[3] == 113
        && regs[5] == (113u ^ 0xFFu)
        && regs[7] == regs[5] + 123;
}