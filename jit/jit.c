#include "jit.h"

#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define JIT_MAX_INSNS 256

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} JitEmit;

static void emit8(JitEmit *emit, uint8_t value) {
    emit->data[emit->len++] = value;
}

static void emit32(JitEmit *emit, uint32_t value) {
    emit8(emit, (uint8_t)(value >> 0));
    emit8(emit, (uint8_t)(value >> 8));
    emit8(emit, (uint8_t)(value >> 16));
    emit8(emit, (uint8_t)(value >> 24));
}

static void patch32(JitEmit *emit, size_t offset, int32_t value) {
    emit->data[offset + 0] = (uint8_t)(value >> 0);
    emit->data[offset + 1] = (uint8_t)(value >> 8);
    emit->data[offset + 2] = (uint8_t)(value >> 16);
    emit->data[offset + 3] = (uint8_t)(value >> 24);
}

static void emit_push_r14(JitEmit *emit) {
    emit8(emit, 0x41);
    emit8(emit, 0x56);
}

static void emit_pop_r14(JitEmit *emit) {
    emit8(emit, 0x41);
    emit8(emit, 0x5E);
}

static void emit_sub_rsp_8(JitEmit *emit) {
    emit8(emit, 0x48);
    emit8(emit, 0x83);
    emit8(emit, 0xEC);
    emit8(emit, 0x08);
}

static void emit_add_rsp_8(JitEmit *emit) {
    emit8(emit, 0x48);
    emit8(emit, 0x83);
    emit8(emit, 0xC4);
    emit8(emit, 0x08);
}

static void emit_load_regs(JitEmit *emit) {
    emit8(emit, 0x4C);
    emit8(emit, 0x8B);
    emit8(emit, 0xB7);
    emit32(emit, offsetof(Cpu, regs));
}

static void emit_load_pc(JitEmit *emit) {
    emit8(emit, 0x8B);
    emit8(emit, 0x87);
    emit32(emit, offsetof(Cpu, pc));
}

static void emit_store_pc(JitEmit *emit) {
    emit8(emit, 0x89);
    emit8(emit, 0x87);
    emit32(emit, offsetof(Cpu, pc));
}

static JitPage *jit_new_page(Jit *jit) {
    if (jit->page_count == jit->page_cap)
        return NULL;

    uint8_t *code = mmap(NULL, jit->page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (code == MAP_FAILED)
        return NULL;

    JitPage *page = &jit->pages[jit->page_count++];

    page->code = code;
    page->size = jit->page_size;

    return page;
}

static void emit_load_eax(JitEmit *emit, uint8_t reg) {
    emit8(emit, 0x41);
    emit8(emit, 0x8B);
    emit8(emit, 0x86);
    emit32(emit, (uint32_t)reg * 4);
}

static void emit_load_ecx(JitEmit *emit, uint8_t reg) {
    emit8(emit, 0x41);
    emit8(emit, 0x8B);
    emit8(emit, 0x8E);
    emit32(emit, (uint32_t)reg * 4);
}

static void emit_store_eax(JitEmit *emit, uint8_t reg) {
    emit8(emit, 0x41);
    emit8(emit, 0x89);
    emit8(emit, 0x86);
    emit32(emit, (uint32_t)reg * 4);
}

static void emit_mov_eax_imm(JitEmit *emit, uint32_t value) {
    emit8(emit, 0xB8);
    emit32(emit, value);
}

static void emit_mov_esi_imm(JitEmit *emit, uint32_t value) {
    emit8(emit, 0xBE);
    emit32(emit, value);
}

static void emit_mov_edx_imm(JitEmit *emit, uint32_t value) {
    emit8(emit, 0xBA);
    emit32(emit, value);
}

static void emit_mov_ecx_imm(JitEmit *emit, uint32_t value) {
    emit8(emit, 0xB9);
    emit32(emit, value);
}

static void emit_mov_edx_eax(JitEmit *emit) {
    emit8(emit, 0x89);
    emit8(emit, 0xC2);
}

static void emit_add_eax_ecx(JitEmit *emit) {
    emit8(emit, 0x01);
    emit8(emit, 0xC8);
}

static void emit_sub_eax_ecx(JitEmit *emit) {
    emit8(emit, 0x29);
    emit8(emit, 0xC8);
}

static void emit_imul_eax_ecx(JitEmit *emit) {
    emit8(emit, 0x0F);
    emit8(emit, 0xAF);
    emit8(emit, 0xC1);
}

static void emit_and_eax_ecx(JitEmit *emit) {
    emit8(emit, 0x21);
    emit8(emit, 0xC8);
}

static void emit_or_eax_ecx(JitEmit *emit) {
    emit8(emit, 0x09);
    emit8(emit, 0xC8);
}

static void emit_xor_eax_ecx(JitEmit *emit) {
    emit8(emit, 0x31);
    emit8(emit, 0xC8);
}

static void emit_add_eax_imm(JitEmit *emit, int32_t value) {
    emit8(emit, 0x05);
    emit32(emit, (uint32_t)value);
}

static void emit_sub_eax_imm(JitEmit *emit, int32_t value) {
    emit8(emit, 0x2D);
    emit32(emit, (uint32_t)value);
}

static void emit_and_eax_imm(JitEmit *emit, uint32_t value) {
    emit8(emit, 0x25);
    emit32(emit, value);
}

static void emit_or_eax_imm(JitEmit *emit, uint32_t value) {
    emit8(emit, 0x0D);
    emit32(emit, value);
}

static void emit_xor_eax_imm(JitEmit *emit, uint32_t value) {
    emit8(emit, 0x35);
    emit32(emit, value);
}

static void emit_shl_eax_cl(JitEmit *emit) {
    emit8(emit, 0xD3);
    emit8(emit, 0xE0);
}

static void emit_sar_eax_cl(JitEmit *emit) {
    emit8(emit, 0xD3);
    emit8(emit, 0xF8);
}

static void emit_shr_eax_cl(JitEmit *emit) {
    emit8(emit, 0xD3);
    emit8(emit, 0xE8);
}

static void emit_call_abs(JitEmit *emit, void *fn) {
    uintptr_t address = (uintptr_t)fn;

    emit8(emit, 0x48);
    emit8(emit, 0xB8);
    emit32(emit, (uint32_t)address);
    emit32(emit, (uint32_t)(address >> 32));
    emit8(emit, 0xFF);
    emit8(emit, 0xD0);
}

static void emit_return(JitEmit *emit) {
    emit_pop_r14(emit);
    emit8(emit, 0xC3);
}

static void emit_exit(JitEmit *emit, JitExit exit) {
    emit_mov_eax_imm(emit, exit);
    emit_return(emit);
}

static JitExit jit_call(Cpu *cpu, uint32_t target, uint32_t return_pc) {
    uint32_t sp = cpu->regs[SP];

    if (sp < 4) {
        return JIT_EXIT_FAULT;
    }

    sp -= 4;

    CpuMemResult result = cpu_store32(cpu, sp, return_pc);

    if (result == CPU_MEM_TLB_MISS)
        return JIT_EXIT_TLB_MISS;

    if (result != CPU_MEM_OK)
        return JIT_EXIT_FAULT;

    cpu->regs[SP] = sp;
    cpu->pc = target;

    return JIT_EXIT_NEXT;
}

static bool emit_call(JitEmit *emit, uint32_t pc, const Instr *instr) {
    uint32_t target = instr->opcode == OP_CALLA
        ? instr->imm
        : pc + (uint32_t)sign_extend(instr->imm, 26);

    emit_mov_esi_imm(emit, target);
    emit_mov_edx_imm(emit, pc + 4);

    emit_sub_rsp_8(emit);
    emit_call_abs(emit, jit_call);
    emit_add_rsp_8(emit);

    emit_return(emit);

    return true;
}

static void emit_address(JitEmit *emit, const Instr *instr) {
    emit_load_eax(emit, instr->rn);

    if (instr->is_reg) {
        emit_load_ecx(emit, instr->rm);
        emit_add_eax_ecx(emit);
    } else {
        emit_add_eax_imm(emit, sign_extend(instr->imm, 16));
    }

    emit_mov_edx_eax(emit);
}

static JitExit jit_ldr(Cpu *cpu, uint32_t rd, uint32_t address, uint32_t next_pc) {
    uint32_t value;
    CpuMemResult result = cpu_load32(cpu, address, &value);

    if (result == CPU_MEM_TLB_MISS)
        return JIT_EXIT_TLB_MISS;

    if (result != CPU_MEM_OK)
        return JIT_EXIT_FAULT;

    cpu->regs[rd] = value;
    cpu->pc = next_pc;

    return JIT_EXIT_NEXT;
}

static bool emit_ldr(JitEmit *emit, uint32_t pc, const Instr *instr) {
    emit_address(emit, instr);
    emit_mov_esi_imm(emit, instr->rd);
    emit_mov_ecx_imm(emit, pc + 4);
    emit_call_abs(emit, jit_ldr);
    emit_return(emit);

    return true;
}

static JitExit jit_str(Cpu *cpu, uint32_t rd, uint32_t address, uint32_t next_pc) {
    CpuMemResult result = cpu_store32(cpu, address, cpu->regs[rd]);

    if (result == CPU_MEM_TLB_MISS)
        return JIT_EXIT_TLB_MISS;

    if (result != CPU_MEM_OK)
        return JIT_EXIT_FAULT;

    cpu->pc = next_pc;

    return JIT_EXIT_NEXT;
}

static bool emit_str(JitEmit *emit, uint32_t pc, const Instr *instr) {
    emit_address(emit, instr);
    emit_mov_esi_imm(emit, instr->rd);
    emit_mov_ecx_imm(emit, pc + 4);
    emit_call_abs(emit, jit_str);
    emit_return(emit);

    return true;
}

static JitExit jit_ldrb(Cpu *cpu, uint32_t rd, uint32_t address, uint32_t next_pc) {
    uint32_t value;
    CpuMemResult result = cpu_load8(cpu, address, &value);

    if (result == CPU_MEM_TLB_MISS)
        return JIT_EXIT_TLB_MISS;

    if (result != CPU_MEM_OK)
        return JIT_EXIT_FAULT;

    cpu->regs[rd] = value;
    cpu->pc = next_pc;

    return JIT_EXIT_NEXT;
}

static JitExit jit_strb(Cpu *cpu, uint32_t rd, uint32_t address, uint32_t next_pc) {
    CpuMemResult result = cpu_store8(cpu, address, cpu->regs[rd]);

    if (result == CPU_MEM_TLB_MISS)
        return JIT_EXIT_TLB_MISS;

    if (result != CPU_MEM_OK)
        return JIT_EXIT_FAULT;

    cpu->pc = next_pc;

    return JIT_EXIT_NEXT;
}

static bool emit_ldrb(JitEmit *emit, uint32_t pc, const Instr *instr) {
    emit_address(emit, instr);
    emit_mov_esi_imm(emit, instr->rd);
    emit_mov_ecx_imm(emit, pc + 4);
    emit_sub_rsp_8(emit);
    emit_call_abs(emit, jit_ldrb);
    emit_add_rsp_8(emit);
    emit_return(emit);

    return true;
}

static bool emit_strb(JitEmit *emit, uint32_t pc, const Instr *instr) {
    emit_address(emit, instr);
    emit_mov_esi_imm(emit, instr->rd);
    emit_mov_ecx_imm(emit, pc + 4);
    emit_sub_rsp_8(emit);
    emit_call_abs(emit, jit_strb);
    emit_add_rsp_8(emit);
    emit_return(emit);

    return true;
}

static bool emit_alu(JitEmit *emit, const Instr *instr) {
    emit_load_eax(emit, instr->rn);

    if (instr->is_reg) {
        emit_load_ecx(emit, instr->rm);

        switch (instr->opcode) {
        case OP_ADD:  emit_add_eax_ecx(emit); break;
        case OP_SUB:  emit_sub_eax_ecx(emit); break;
        case OP_MUL:  emit_imul_eax_ecx(emit); break;
        case OP_AND:  emit_and_eax_ecx(emit); break;
        case OP_OR:   emit_or_eax_ecx(emit); break;
        case OP_XOR:  emit_xor_eax_ecx(emit); break;
        case OP_SHL:  emit_shl_eax_cl(emit); break;
        case OP_SHR:  emit_sar_eax_cl(emit); break;
        case OP_SHRU: emit_shr_eax_cl(emit); break;
        default: return false;
        }
    } else {
        switch (instr->opcode) {
        case OP_ADD:
            emit_add_eax_imm(emit, sign_extend(instr->imm, 16));
            break;

        case OP_SUB:
            emit_sub_eax_imm(emit, sign_extend(instr->imm, 16));
            break;

        case OP_AND:
            emit_and_eax_imm(emit, instr->imm);
            break;

        case OP_OR:
            emit_or_eax_imm(emit, instr->imm);
            break;

        case OP_XOR:
            emit_xor_eax_imm(emit, instr->imm);
            break;

        default:
            return false;
        }
    }

    emit_store_eax(emit, instr->rd);

    return true;
}

static bool emit_branch(JitEmit *emit, uint32_t pc, const Instr *instr) {
    emit_load_eax(emit, instr->rn);
    emit_load_ecx(emit, instr->rd);

    emit8(emit, 0x39);
    emit8(emit, 0xC8);

    uint8_t jcc;

    switch (instr->cond) {
    case COND_JEQ:  jcc = 0x84; break;
    case COND_JNE:  jcc = 0x85; break;
    case COND_JLT:  jcc = 0x8C; break;
    case COND_JGE:  jcc = 0x8D; break;
    case COND_JLTU: jcc = 0x82; break;
    case COND_JGEU: jcc = 0x83; break;
    default: return false;
    }

    emit8(emit, 0x0F);
    emit8(emit, jcc);

    size_t branch_offset = emit->len;
    emit32(emit, 0);

    emit_mov_eax_imm(emit, pc + 4);
    emit_store_pc(emit);
    emit_exit(emit, JIT_EXIT_NEXT);

    size_t target_offset = emit->len;

    uint32_t target = instr->is_absolute
        ? instr->imm
        : pc + (uint32_t)sign_extend(instr->imm, 12);

    emit_mov_eax_imm(emit, target);
    emit_store_pc(emit);
    emit_exit(emit, JIT_EXIT_NEXT);

    patch32(emit, branch_offset, (int32_t)(target_offset - (branch_offset + 4)));

    return true;
}

static bool emit_jump(JitEmit *emit, uint32_t pc, const Instr *instr) {
    uint32_t target = instr->opcode == OP_JMPA
        ? instr->imm
        : pc + (uint32_t)sign_extend(instr->imm, 26);

    emit_mov_eax_imm(emit, target);
    emit_store_pc(emit);
    emit_exit(emit, JIT_EXIT_NEXT);

    return true;
}

static JitExit jit_ret(Cpu *cpu) {
    uint32_t sp = cpu->regs[SP];
    uint32_t pc;
    CpuMemResult result = cpu_load32(cpu, sp, &pc);

    if (result == CPU_MEM_TLB_MISS)
        return JIT_EXIT_TLB_MISS;

    if (result != CPU_MEM_OK)
        return JIT_EXIT_FAULT;

    cpu->regs[SP] = sp + 4;
    cpu->pc = pc;

    return JIT_EXIT_NEXT;
}

static bool emit_ret_instruction(JitEmit *emit) {
    emit_sub_rsp_8(emit);
    emit_call_abs(emit, jit_ret);
    emit_add_rsp_8(emit);

    emit_return(emit);

    return true;
}

static bool emit_instruction(JitEmit *emit, uint32_t pc, const Instr *instr, bool *terminate) {
    switch (instr->opcode) {
    case OP_NOP:
        return true;

    case OP_ADD:
    case OP_SUB:
    case OP_MUL:
    case OP_AND:
    case OP_OR:
    case OP_XOR:
    case OP_SHL:
    case OP_SHR:
    case OP_SHRU:
        return emit_alu(emit, instr);

    case OP_JMP:
    case OP_JMPA:
        *terminate = true;
        return emit_jump(emit, pc, instr);

    case OP_JXX:
        *terminate = true;
        return emit_branch(emit, pc, instr);

    case OP_RPC:
        emit_load_pc(emit);
        emit_store_eax(emit, instr->rd);
        return true;

    case OP_CALL:
    case OP_CALLA:
        *terminate = true;
        return emit_call(emit, pc, instr);

    case OP_RET:
        *terminate = true;
        return emit_ret_instruction(emit);

    case OP_LDR:
        *terminate = true;
        return emit_ldr(emit, pc, instr);

    case OP_STR:
        *terminate = true;
        return emit_str(emit, pc, instr);

    case OP_LDRB:
        *terminate = true;
        return emit_ldrb(emit, pc, instr);

    case OP_STRB:
        *terminate = true;
        return emit_strb(emit, pc, instr);

    default:
        return false;
    }
}

static JitFn jit_compile(Jit *jit, Cpu *cpu, uint32_t pc) {
    JitPage *page = jit_new_page(jit);

    if (page == NULL)
        return NULL;

    JitEmit emit = {
        .data = page->code,
        .cap = page->size,
    };

    emit_push_r14(&emit);
    emit_load_regs(&emit);

    uint32_t current_pc = pc;

    for (size_t i = 0; i < JIT_MAX_INSNS; i++) {
        uint32_t word;

        if (jit->fetch(cpu, current_pc, &word) != CPU_MEM_OK)
            return NULL;

        Instr instr = isa_decode(word);
        bool terminate = false;

        if (!emit_instruction(&emit, current_pc, &instr, &terminate))
            return NULL;

        current_pc += 4;

        if (terminate)
            goto finalise;
    }

    emit_mov_eax_imm(&emit, current_pc);
    emit_store_pc(&emit);
    emit_exit(&emit, JIT_EXIT_NEXT);

finalise:
    if (mprotect(page->code, page->size, PROT_READ | PROT_EXEC) != 0)
        return NULL;

    return (JitFn)(void *)page->code;
}

static JitBlock *jit_find(Jit *jit, uint32_t pc) {
    for (size_t i = 0; i < jit->block_count; i++) {
        if (jit->blocks[i].pc == pc)
            return &jit->blocks[i];
    }

    return NULL;
}

static JitBlock *jit_add_block(Jit *jit, uint32_t pc, JitFn fn) {
    if (jit->block_count == jit->block_cap)
        return NULL;

    JitBlock *block = &jit->blocks[jit->block_count++];

    block->pc = pc;
    block->fn = fn;

    return block;
}

bool jit_init(Jit *jit, size_t page_cap, size_t block_cap, JitFetch fetch) {
    *jit = (Jit){0};

    jit->page_size = (size_t)sysconf(_SC_PAGESIZE);

    if (jit->page_size == 0)
        return false;

    jit->pages = calloc(page_cap, sizeof(*jit->pages));

    if (jit->pages == NULL)
        return false;

    jit->blocks = calloc(block_cap, sizeof(*jit->blocks));

    if (jit->blocks == NULL) {
        free(jit->pages);
        *jit = (Jit){0};
        return false;
    }

    jit->page_cap = page_cap;
    jit->block_cap = block_cap;
    jit->fetch = fetch;

    return true;
}

void jit_destroy(Jit *jit) {
    for (size_t i = 0; i < jit->page_count; i++)
        munmap(jit->pages[i].code, jit->pages[i].size);

    free(jit->pages);
    free(jit->blocks);

    *jit = (Jit){0};
}

JitBlock *jit_get_block(Jit *jit, Cpu *cpu, uint32_t pc) {
    JitBlock *block = jit_find(jit, pc);

    if (block != NULL)
        return block;

    JitFn fn = jit_compile(jit, cpu, pc);

    if (fn == NULL)
        return NULL;

    return jit_add_block(jit, pc, fn);
}