#include <stdint.h>
#include <stdio.h>

#include "isa.h"
#include "jit.h"

#define MEMORY_SIZE 0x2000
#define PROGRAM_PC  0x1000

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

static uint32_t encode_jump(Opcode opcode, int32_t offset) {
    return ((uint32_t)opcode << 26)
         | ((uint32_t)offset & 0x03FFFFFF);
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

    write32(memory, 0x1000, encode_reg(OP_ADD, 0, 1, 2));
    write32(memory, 0x1004, encode_jump(OP_JMP, 0xFC));

    write32(memory, 0x1100, encode_reg(OP_ADD, 3, 0, 1));
    write32(memory, 0x1104, encode_jump(OP_JMP, -0x104));

    Jit jit;

    if (!jit_init(&jit, 16, 16, fetch, memory))
        return 1;

    uint32_t pc = PROGRAM_PC;

    for (size_t i = 0; i < 10; i++) {
        JitBlock *block = jit_get_block(&jit, pc);

        if (block == NULL)
            return 1;

        pc = block->fn(regs);
    }

    printf("r0 = %u\n", regs[0]);
    printf("r3 = %u\n", regs[3]);
    printf("pc = 0x%08X\n", pc);
    printf("blocks = %zu\n", jit.block_count);

    jit_destroy(&jit);

    return jit.block_count == 2 && regs[0] == 123;
}