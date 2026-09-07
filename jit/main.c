#include <stdint.h>
#include <stdio.h>

#include "cpu.h"
#include "isa.h"
#include "jit.h"
#include "memory.h"

#define PROGRAM_PC    0x1000
#define RAM_SIZE      0x2000
#define RAM_BASE      0x00020000
#define STACK_TOP     0x00021000
#define BYTE_ADDR     0x00020101
#define BYTE_DST      0x00020103

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

int main(void) {
    uint8_t rom[ORION_ROM_END - ORION_ROM_BASE] = {0};
    uint8_t ihvt[ORION_IHVT_END - ORION_IHVT_BASE] = {0};
    uint8_t ram[RAM_SIZE] = {0};

    Memory memory;

    memory_init(&memory, rom, sizeof(rom), ihvt, sizeof(ihvt), ram, sizeof(ram));

    Cpu cpu = {
        .pc = PROGRAM_PC,
        .flags = 0x00000002,
        .memory = &memory,
    };

    cpu.regs = cpu.ksr;
    cpu.regs[SP] = STACK_TOP;
    cpu.regs[1] = BYTE_ADDR;
    cpu.regs[2] = BYTE_DST;
    cpu.regs[3] = 0;

    ram[BYTE_ADDR - RAM_BASE] = 0xAB;

    write32(rom, 0x0000, encode_reg(OP_LDRB, 0, 1, 3));
    write32(rom, 0x0004, encode_reg(OP_STRB, 0, 2, 3));

    Jit jit;

    if (!jit_init(&jit, 16, 16, cpu_fetch32))
        return 1;

    for (size_t i = 0; i < 2; i++) {
        JitBlock *block = jit_get_block(&jit, &cpu, cpu.pc);

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
    printf("r0 = 0x%08X\n", cpu.regs[0]);
    printf("stored = 0x%02X\n", ram[BYTE_DST - RAM_BASE]);
    printf("blocks = %zu\n", jit.block_count);

    jit_destroy(&jit);

    return cpu.pc == 0x1008
        && cpu.regs[0] == 0xAB
        && ram[BYTE_DST - RAM_BASE] == 0xAB
        && jit.block_count == 2
        ? 0
        : 1;
}