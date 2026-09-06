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
    emit8(emit, 0x8B);
    emit8(emit, 0x87);
    emit32(emit, (uint32_t)reg * 4);
}

static void emit_load_ecx(JitEmit *emit, uint8_t reg) {
    emit8(emit, 0x8B);
    emit8(emit, 0x8F);
    emit32(emit, (uint32_t)reg * 4);
}

static void emit_store_eax(JitEmit *emit, uint8_t reg) {
    emit8(emit, 0x89);
    emit8(emit, 0x87);
    emit32(emit, (uint32_t)reg * 4);
}

static void emit_mov_eax_imm(JitEmit *emit, uint32_t value) {
    emit8(emit, 0xB8);
    emit32(emit, value);
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

static void emit_ret(JitEmit *emit) {
    emit8(emit, 0xC3);
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
    default:
        return false;
    }

    emit8(emit, 0x0F);
    emit8(emit, jcc);

    size_t branch_offset = emit->len;
    emit32(emit, 0);

    emit_mov_eax_imm(emit, pc + 4);
    emit_ret(emit);

    size_t target_offset = emit->len;

    uint32_t target = instr->is_absolute
        ? instr->imm
        : pc + (uint32_t)sign_extend(instr->imm, 12);

    emit_mov_eax_imm(emit, target);
    emit_ret(emit);

    patch32(emit, branch_offset, (int32_t)(target_offset - (branch_offset + 4)));

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
    case OP_JMPA: {
        uint32_t target = instr->opcode == OP_JMPA
            ? instr->imm
            : pc + (uint32_t)sign_extend(instr->imm, 26);

        emit_mov_eax_imm(emit, target);
        emit_ret(emit);

        *terminate = true;

        return true;
    }

    case OP_JXX:
        *terminate = true;
        return emit_branch(emit, pc, instr);

    default:
        return false;
    }
}

bool jit_init(Jit *jit, size_t page_cap, JitFetch fetch, void *fetch_ctx) {
    *jit = (Jit){0};

    jit->page_size = (size_t)sysconf(_SC_PAGESIZE);

    if (jit->page_size == 0)
        return false;

    jit->pages = calloc(page_cap, sizeof(*jit->pages));

    if (jit->pages == NULL)
        return false;

    jit->page_cap = page_cap;
    jit->fetch = fetch;
    jit->fetch_ctx = fetch_ctx;

    return true;
}

void jit_destroy(Jit *jit) {
    for (size_t i = 0; i < jit->page_count; i++)
        munmap(jit->pages[i].code, jit->pages[i].size);

    free(jit->pages);

    *jit = (Jit){0};
}

JitFn jit_compile(Jit *jit, uint32_t pc) {
    JitPage *page = jit_new_page(jit);

    if (page == NULL)
        return NULL;

    JitEmit emit = {
        .data = page->code,
        .cap = page->size,
    };

    uint32_t current_pc = pc;

    for (size_t i = 0; i < JIT_MAX_INSNS; i++) {
        Instr instr = isa_decode(jit->fetch(jit->fetch_ctx, current_pc));
        bool terminate = false;

        if (!emit_instruction(&emit, current_pc, &instr, &terminate))
            return NULL;

        current_pc += 4;

        if (terminate)
            goto finalise;
    }

    emit_mov_eax_imm(&emit, current_pc);
    emit_ret(&emit);

finalise:
    if (mprotect(page->code, page->size, PROT_READ | PROT_EXEC) != 0)
        return NULL;

    return (JitFn)(void *)page->code;
}