#include <stdint.h>
#include <stdio.h>

#include "cpu.h"
#include "isa.h"
#include "jit.h"

#define MEMORY_SIZE 0x2000
#define PROGRAM_PC  0x1000
#define STACK_TOP   0x1800
#define DATA_ADDR   0x1200
#define STORE_ADDR  0x1204

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
    cpu.regs[1] = DATA_ADDR;
    cpu.regs[2] = STORE_ADDR;
    cpu.regs[3] = 0;

    write32(memory, DATA_ADDR, 1234);

    write32(memory, 0x1000, encode_reg(OP_LDR, 0, 1, 3));
    write32(memory, 0x1004, encode_reg(OP_STR, 0, 2, 3));

    Jit jit;

    if (!jit_init(&jit, 16, 16, fetch, memory))
        return 1;

    for (size_t i = 0; i < 2; i++) {
        JitBlock *block = jit_get_block(&jit, cpu.pc);

        if (block == NULL) {
            printf("failed to compile block at 0x%08X\n", cpu.pc);
            return 1;
        }

        JitExit exit = block->fn(&cpu);

        if (exit != JIT_EXIT_NEXT) {
            printf("jit exit = %d at 0x%08X\n", exit, block->pc);
            return 1;
        }
    }

    printf("pc = 0x%08X\n", cpu.pc);
    printf("sp = 0x%08X\n", cpu.regs[SP]);
    printf("r0 = %u\n", cpu.regs[0]);
    printf("stored = %u\n", read32(memory, STORE_ADDR));
    printf("blocks = %zu\n", jit.block_count);

    jit_destroy(&jit);

    return cpu.pc == 0x1008
        && cpu.regs[0] == 1234
        && read32(memory, STORE_ADDR) == 1234
        && jit.block_count == 2
        ? 0
        : 1;
}