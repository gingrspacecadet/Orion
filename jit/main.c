#include <stdint.h>
#include <stdio.h>

#include "cpu.h"
#include "isa.h"
#include "jit.h"
#include "memory.h"

#define RAM_SIZE 0x2000
#define RAM_BASE 0x00020000

#define PROGRAM_PC 0x00001000
#define STACK_TOP 0x00021000

#define VIRTUAL_SRC 0x00030000
#define VIRTUAL_DST 0x00031000

#define PHYSICAL_SRC 0x00020000
#define PHYSICAL_DST 0x00021000

#define FLAG_PRV (1u << 1)
#define FLAG_VM  (1u << 2)

#define TLB_VALID (1u << 0)
#define TLB_W     (1u << 3)
#define TLB_U     (1u << 2)

static void write32(uint8_t *memory, uint32_t offset, uint32_t value) {
    memory[offset + 0] = (uint8_t)(value >> 0);
    memory[offset + 1] = (uint8_t)(value >> 8);
    memory[offset + 2] = (uint8_t)(value >> 16);
    memory[offset + 3] = (uint8_t)(value >> 24);
}

static uint32_t encode_reg(Opcode opcode, uint8_t rd, uint8_t rn, uint8_t rm) {
    return ((uint32_t)opcode << 26) |
           ((uint32_t)rn << 22) |
           ((uint32_t)rd << 18) |
           ((uint32_t)rm << 2) |
           (1u << 1);
}

static void test_identity_mapping(Cpu *cpu) {
    uint32_t value = 0;

    cpu->flags |= FLAG_VM;
    cpu->flags |= FLAG_PRV;
    cpu->asid = 0;

    cpu->tlb[0].hi = (0x00020u << 12) | TLB_VALID;
    cpu->tlb[0].lo = (0x00020u << 12) | TLB_W | TLB_U;

    if (cpu_store32(cpu, 0x00020000, 0x12345678) != CPU_MEM_OK) {
        printf("identity store failed\n");
        return;
    }

    if (cpu_load32(cpu, 0x00020000, &value) != CPU_MEM_OK) {
        printf("identity load failed\n");
        return;
    }

    printf("identity: %s (%08x)\n",
        value == 0x12345678 ? "PASS" : "FAIL",
        value);
}

static void test_remapping(Cpu *cpu) {
    uint32_t value = 0;

    cpu->tlb[0].hi = (0x00030u << 12) | TLB_VALID;
    cpu->tlb[0].lo = (0x00020u << 12) | TLB_W | TLB_U;

    if (cpu_store32(cpu, 0x00030000, 0xdeadbeef) != CPU_MEM_OK) {
        printf("remap store failed\n");
        return;
    }

    if (cpu_load32(cpu, 0x00030000, &value) != CPU_MEM_OK) {
        printf("remap load failed\n");
        return;
    }

    printf("remap: %s (%08x)\n",
        value == 0xdeadbeef ? "PASS" : "FAIL",
        value);

    printf("physical backing: %02x %02x %02x %02x\n",
        cpu->memory->ram[0],
        cpu->memory->ram[1],
        cpu->memory->ram[2],
        cpu->memory->ram[3]);
}

static void test_page_offset(Cpu *cpu) {
    uint32_t value = 0;

    cpu->tlb[0].hi = (0x00030u << 12) | TLB_VALID;
    cpu->tlb[0].lo = (0x00020u << 12) | TLB_W | TLB_U;

    if (cpu_store32(cpu, 0x00030ffcu, 0xaabbccdd) != CPU_MEM_OK) {
        printf("offset store failed\n");
        return;
    }

    if (cpu_load32(cpu, 0x00030ffcu, &value) != CPU_MEM_OK) {
        printf("offset load failed\n");
        return;
    }

    printf("page offset: %s (%08x)\n",
        value == 0xaabbccdd ? "PASS" : "FAIL",
        value);
}

static void test_tlb_miss(Cpu *cpu) {
    uint32_t value = 0;

    cpu->tlb[0].hi = 0;
    cpu->tlb[0].lo = 0;

    CpuMemResult result = cpu_load32(cpu, 0x00030000, &value);

    printf("tlb miss: %s\n",
        result == CPU_MEM_TLB_MISS ? "PASS" : "FAIL");
}

static void test_invalid_entry(Cpu *cpu) {
    uint32_t value = 0;

    cpu->tlb[0].hi = (0x00030u << 12);
    cpu->tlb[0].lo = (0x00020u << 12) | TLB_W | TLB_U;

    CpuMemResult result = cpu_load32(cpu, 0x00030000, &value);

    printf("invalid entry: %s\n",
        result == CPU_MEM_TLB_MISS ? "PASS" : "FAIL");
}

static void test_write_protection(Cpu *cpu) {
    cpu->tlb[0].hi = (0x00030u << 12) | TLB_VALID;
    cpu->tlb[0].lo = (0x00020u << 12) | TLB_U;

    CpuMemResult result = cpu_store32(cpu, 0x00030000, 0xabcdef01);

    printf("write protection: %s\n",
        result == CPU_MEM_FAULT ? "PASS" : "FAIL");
}

static void test_user_protection(Cpu *cpu) {
    uint32_t value = 0;

    cpu->flags &= ~FLAG_PRV;

    cpu->tlb[0].hi = (0x00030u << 12) | TLB_VALID;
    cpu->tlb[0].lo = (0x00020u << 12) | TLB_W;

    CpuMemResult result = cpu_load32(cpu, 0x00030000, &value);

    printf("user protection: %s\n",
        result == CPU_MEM_FAULT ? "PASS" : "FAIL");

    cpu->flags |= FLAG_PRV;
}

static void test_supervisor_access(Cpu *cpu) {
    uint32_t value = 0;

    cpu->flags |= FLAG_PRV;

    cpu->tlb[0].hi = (0x00030u << 12) | TLB_VALID;
    cpu->tlb[0].lo = (0x00020u << 12) | TLB_W;

    if (cpu_store32(cpu, 0x00030000, 0x55667788) != CPU_MEM_OK) {
        printf("supervisor store failed\n");
        return;
    }

    if (cpu_load32(cpu, 0x00030000, &value) != CPU_MEM_OK) {
        printf("supervisor load failed\n");
        return;
    }

    printf("supervisor access: %s (%08x)\n",
        value == 0x55667788 ? "PASS" : "FAIL",
        value);
}

static void test_asid_isolation(Cpu *cpu) {
    uint32_t value = 0;

    cpu->tlb[0].hi = (0x00030u << 12) | (1u << 1) | TLB_VALID;
    cpu->tlb[0].lo = (0x00020u << 12) | TLB_W | TLB_U;

    cpu->asid = 0;

    CpuMemResult result = cpu_load32(cpu, 0x00030000, &value);

    printf("asid isolation: %s\n",
        result == CPU_MEM_TLB_MISS ? "PASS" : "FAIL");

    cpu->asid = 1;

    if (cpu_load32(cpu, 0x00030000, &value) != CPU_MEM_OK) {
        printf("asid match failed\n");
        return;
    }

    printf("asid match: PASS\n");
}

static void test_multiple_tlb_entries(Cpu *cpu) {
    uint32_t value = 0;

    cpu->asid = 0;

    cpu->tlb[0].hi = (0x00030u << 12) | TLB_VALID;
    cpu->tlb[0].lo = (0x00020u << 12) | TLB_W | TLB_U;

    cpu->tlb[1].hi = (0x00031u << 12) | TLB_VALID;
    cpu->tlb[1].lo = (0x00021u << 12) | TLB_W | TLB_U;

    if (cpu_store32(cpu, 0x00030000, 0x11111111) != CPU_MEM_OK) {
        printf("multi-entry first store failed\n");
        return;
    }

    if (cpu_store32(cpu, 0x00031000, 0x22222222) != CPU_MEM_OK) {
        printf("multi-entry second store failed\n");
        return;
    }

    if (cpu_load32(cpu, 0x00031000, &value) != CPU_MEM_OK) {
        printf("multi-entry second load failed\n");
        return;
    }

    printf("multiple entries: %s (%08x)\n",
        value == 0x22222222 ? "PASS" : "FAIL",
        value);
}

static void test_vm_disabled(Cpu *cpu) {
    uint32_t value = 0;

    cpu->flags &= ~FLAG_VM;
    cpu->asid = 999;

    for (size_t i = 0; i < TLB_ENTRIES; i++) {
        cpu->tlb[i].hi = 0;
        cpu->tlb[i].lo = 0;
    }

    CpuMemResult result = cpu_store32(cpu, RAM_BASE, 0xfeedface);

    if (result != CPU_MEM_OK) {
        printf("vm disabled store failed\n");
        return;
    }

    result = cpu_load32(cpu, RAM_BASE, &value);

    printf("vm disabled: %s (%08x)\n",
        result == CPU_MEM_OK && value == 0xfeedface ? "PASS" : "FAIL",
        value);

    cpu->flags |= FLAG_VM;
}

static void test_jit_mmu(Cpu *cpu, Jit *jit) {
    uint32_t virtual_src = VIRTUAL_SRC;
    uint32_t virtual_dst = VIRTUAL_DST;
    uint32_t word = 0;

    cpu->flags |= FLAG_VM;
    cpu->flags |= FLAG_PRV;
    cpu->asid = 0;

    for (size_t i = 0; i < TLB_ENTRIES; i++) {
        cpu->tlb[i].hi = 0;
        cpu->tlb[i].lo = 0;
    }

    cpu->tlb[0].hi = (VIRTUAL_SRC & ~PAGE_MASK) | TLB_VALID;
    cpu->tlb[0].lo = (PHYSICAL_SRC & ~PAGE_MASK) | TLB_W | TLB_U;

    cpu->tlb[1].hi = (VIRTUAL_DST & ~PAGE_MASK) | TLB_VALID;
    cpu->tlb[1].lo = (PHYSICAL_DST & ~PAGE_MASK) | TLB_W | TLB_U;

    cpu->tlb[2].hi = (PROGRAM_PC & ~PAGE_MASK) | TLB_VALID;
    cpu->tlb[2].lo = 0x00001000u | TLB_W | TLB_U;

    uint32_t source_value = 0x13572468;

    write32(
        cpu->memory->ram,
        PHYSICAL_SRC - RAM_BASE,
        source_value
    );

    cpu->regs[1] = virtual_src;
    cpu->regs[2] = virtual_dst;
    cpu->regs[3] = 0;

    write32(cpu->memory->rom, 0x0000, encode_reg(OP_LDR, 3, 1, 0));
    write32(cpu->memory->rom, 0x0004, encode_reg(OP_STR, 3, 2, 0));

    cpu->pc = PROGRAM_PC;

    printf("jit mmu: fetch test: ");

    CpuMemResult fetch_result = cpu_fetch32(cpu, cpu->pc, &word);

    if (fetch_result != CPU_MEM_OK) {
        printf("FAIL (result=%d)\n", fetch_result);
        return;
    }

    printf("PASS (%08x)\n", word);

    printf("jit mmu: compile test: ");

    JitBlock *block = jit_get_block(jit, cpu, cpu->pc);

    if (!block) {
        printf("FAIL\n");
        return;
    }

    printf("PASS\n");

    JitExit exit = block->fn(cpu);

    if (exit != JIT_EXIT_NEXT) {
        printf("jit mmu: load exit %d\n", exit);
        return;
    }

    JitBlock *next = jit_get_block(jit, cpu, cpu->pc);

    if (!next) {
        printf("jit mmu: second compile failed\n");
        return;
    }

    exit = next->fn(cpu);

    if (exit != JIT_EXIT_NEXT) {
        printf("jit mmu: store exit %d\n", exit);
        return;
    }

    uint32_t result = 0;

    CpuMemResult mem_result = cpu_load32(cpu, VIRTUAL_DST, &result);

    printf("jit mmu: %s (%08x)\n",
        mem_result == CPU_MEM_OK && result == source_value ? "PASS" : "FAIL",
        result);
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
        .pc = PROGRAM_PC,
        .flags = FLAG_PRV,
        .memory = &memory,
        .asid = 0,
    };

    cpu.regs = cpu.ksr;
    cpu.regs[15] = STACK_TOP;

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
    test_page_offset(&cpu);
    test_tlb_miss(&cpu);
    test_invalid_entry(&cpu);
    test_write_protection(&cpu);
    test_user_protection(&cpu);
    test_supervisor_access(&cpu);
    test_asid_isolation(&cpu);
    test_multiple_tlb_entries(&cpu);
    test_vm_disabled(&cpu);

    printf("\nJIT:\n");

    Jit jit;

    if (!jit_init(&jit, 16, 16, cpu_fetch32)) {
        printf("jit init failed\n");
        return 1;
    }

    test_jit_mmu(&cpu, &jit);

    return 0;
}