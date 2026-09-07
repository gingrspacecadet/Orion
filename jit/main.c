#include <stdint.h>
#include <stdio.h>

#include "cpu.h"
#include "isa.h"
#include "jit.h"

#define MEMORY_SIZE 0x2000
#define PROGRAM_PC  0x1000
#define STACK_TOP   0x1800

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

static uint32_t read32(const uint8_t *memory, uint32_t address) {
    return (uint32_t)memory[address + 0]
         | (uint32_t)memory[address + 1] << 8
         | (uint32_t)memory[address + 2] << 16
         | (uint32_t)memory[address + 3] << 24;
}

int main(void) {
    uint8_t memory[MEMORY_SIZE] = {0};

    Cpu cpu = {
        .pc = PROGRAM_PC,
        .flags = 0x00000002,
        .memory = memory,
        .memory_size = sizeof(memory),
    };

    cpu.regs = cpu.ksr;

    cpu.regs[SP] = STACK_TOP;
    cpu.regs[1] = 100;
    cpu.regs[2] = 23;

    write32(memory, 0x1000, encode_jump(OP_CALL, 0x100));
    write32(memory, 0x1004, encode_jump(OP_JMP, 0));

    write32(memory, 0x1100, encode_reg(OP_ADD, 0, 1, 2));
    write32(memory, 0x1104, ((uint32_t)OP_RET << 26));

    Jit jit;

    if (!jit_init(&jit, 16, 16, fetch, memory))
        return 1;

    for (size_t i = 0; i < 2; i++) {
        JitBlock *block = jit_get_block(&jit, cpu.pc);

        if (block == NULL)
            return 1;

        if (block->fn(&cpu) != JIT_EXIT_NEXT)
            return 1;
    }

    printf("r0 = %u\n", cpu.regs[0]);
    printf("sp = 0x%08X\n", cpu.regs[SP]);
    printf("pc = 0x%08X\n", cpu.pc);
    printf("blocks = %zu\n", jit.block_count);

    jit_destroy(&jit);

    return 0;
}