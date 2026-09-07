#include <stdint.h>
#include <stdio.h>

#include "cpu.h"
#include "memory.h"

#define RAM_SIZE 0x2000
#define RAM_BASE 0x00020000

static void test_identity_mapping(Cpu *cpu) {
    uint32_t value = 0;

    cpu->flags |= (1u << 2);

    cpu->tlb[0].hi = (0x00020u << 12) | 1u;
    cpu->tlb[0].lo = (0x00020u << 12) | (1u << 3) | (1u << 2);

    if (cpu_store32(cpu, 0x00020000, 0x12345678) != CPU_MEM_OK) {
        printf("identity store failed\n");
        return;
    }

    if (cpu_load32(cpu, 0x00020000, &value) != CPU_MEM_OK) {
        printf("identity load failed\n");
        return;
    }

    printf("identity: %08x\n", value);
}

static void test_remapping(Cpu *cpu) {
    uint32_t value = 0;

    cpu->tlb[0].hi = (0x00030u << 12) | 1u;
    cpu->tlb[0].lo = (0x00020u << 12) | (1u << 3) | (1u << 2);

    if (cpu_store32(cpu, 0x00030000, 0xdeadbeef) != CPU_MEM_OK) {
        printf("remap store failed\n");
        return;
    }

    if (cpu_load32(cpu, 0x00030000, &value) != CPU_MEM_OK) {
        printf("remap load failed\n");
        return;
    }

    printf("remap: %08x\n", value);

    printf("physical backing: %02x %02x %02x %02x\n",
        cpu->memory->ram[0],
        cpu->memory->ram[1],
        cpu->memory->ram[2],
        cpu->memory->ram[3]);
}

static void test_tlb_miss(Cpu *cpu) {
    uint32_t value = 0;

    cpu->tlb[0].hi = 0;
    cpu->tlb[0].lo = 0;

    CpuMemResult result = cpu_load32(cpu, 0x00030000, &value);

    printf("tlb miss: %s\n",
        result == CPU_MEM_TLB_MISS ? "PASS" : "FAIL");
}

static void test_write_protection(Cpu *cpu) {
    cpu->tlb[0].hi = (0x00030u << 12) | 1u;
    cpu->tlb[0].lo = (0x00020u << 12) | (1u << 2);

    CpuMemResult result = cpu_store32(cpu, 0x00030000, 0xabcdef01);

    printf("write protection: %s\n",
        result == CPU_MEM_FAULT ? "PASS" : "FAIL");
}

static void test_user_protection(Cpu *cpu) {
    uint32_t value = 0;

    cpu->flags &= ~(1u << 1);

    cpu->tlb[0].hi = (0x00030u << 12) | 1u;
    cpu->tlb[0].lo = (0x00020u << 12) | (1u << 3);

    CpuMemResult result = cpu_load32(cpu, 0x00030000, &value);

    printf("user protection: %s\n",
        result == CPU_MEM_FAULT ? "PASS" : "FAIL");

    cpu->flags |= (1u << 1);
}

int main(void) {
    uint8_t rom[ORION_ROM_END - ORION_ROM_BASE] = {0};
    uint8_t ihvt[ORION_IHVT_END - ORION_IHVT_BASE] = {0};
    uint8_t ram[RAM_SIZE] = {0};

    Memory memory;

    memory_init(
        &memory,
        rom,
        sizeof(rom),
        ihvt,
        sizeof(ihvt),
        ram,
        sizeof(ram)
    );

    Cpu cpu = {
        .pc = 0x1000,
        .flags = (1u << 1),
        .memory = &memory,
        .asid = 0,
    };

    cpu.regs = cpu.ksr;

    printf("MMU disabled:\n");

    uint32_t value = 0;

    if (cpu_store32(&cpu, RAM_BASE, 0x11223344) == CPU_MEM_OK &&
        cpu_load32(&cpu, RAM_BASE, &value) == CPU_MEM_OK) {
        printf("physical: %08x\n", value);
    } else {
        printf("physical access failed\n");
    }

    printf("\nMMU enabled:\n");

    test_identity_mapping(&cpu);
    test_remapping(&cpu);
    test_tlb_miss(&cpu);
    test_write_protection(&cpu);
    test_user_protection(&cpu);

    return 0;
}