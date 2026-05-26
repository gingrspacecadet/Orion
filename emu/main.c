#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "isa.h"
#include "mem.h"

typedef struct {
    uint8_t opcode;
    uint8_t rn;
    uint8_t rd;
    bool is_reg;
    bool is_signed;
    uint16_t imm;
    uint8_t rm;

    // J-type
    uint8_t cond;
    bool is_absolute;

    // FLAGS
    bool is_write;
} Instr;

static uint8_t *load_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); exit(1); }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { perror("ftell"); exit(1); }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { perror("malloc"); exit(1); }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { perror("fread"); exit(1); }
    fclose(f);
    *out_len = (size_t)sz;
    return buf;
}

static int opcode_type(uint8_t opcode) {
    switch (opcode) {
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_SHL:
        case OP_SHR:
        case OP_AND:
        case OP_OR:
        case OP_NOT:
        case OP_XOR:
        case OP_LUI:
        case OP_CMP: return TYPE_A;

        case OP_LDR:
        case OP_STR:
        case OP_LDRB:
        case OP_STRB:
        case OP_PUSH:
        case OP_POP: return TYPE_M;

        case OP_JXX:
        case OP_CALL:
        case OP_RET:
        case OP_ICALL:
        case OP_IRET: return TYPE_J;

        case OP_FLAGS: return TYPE_S;

        default: return -1;
    }
}

static Instr decode(uint32_t word) {
    Instr instr = {0};
    instr.opcode = (word >> 26) & 0x3F;
    
    // same with all opcodes
    instr.is_reg = (word >> IS_REG_SHIFT) & IS_REG_MASK;
    instr.is_signed = (word >> SIGNED_SHIFT) & SIGNED_MASK;

    if (instr.is_reg) {
        instr.rm = (word >> IMM_RM_SHIFT) & RM_MASK;
    } else {
        instr.imm = (word >> IMM_RM_SHIFT) & IMM_MASK;
    }

    switch (opcode_type(instr.opcode)) {
        case TYPE_M:
        case TYPE_A: {
            instr.rn = (word >> RN_SHIFT) & RN_MASK;
            instr.rd = (word >> RD_SHIFT) & RD_MASK;
            break;
        }

        case TYPE_J: {
            instr.cond = (word >> COND_SHIFT) & COND_MASK;
            instr.is_absolute = (word >> ABS_SHIFT) & ABS_MASK;
            // TODO: raise exception if `reserved` non-zero
            break;
        }

        case TYPE_S: {
            instr.is_write = word & 0x1;
            if (instr.is_write) {
                if (instr.is_reg) {
                    instr.rm = (word >> 10) & 0x3F;
                } else {
                    instr.imm = (word >> 10) & 0xFFFF;
                }
            } else {
                instr.rd = (word >> 10) & 0x3F;
            }
            break;
        }

        default: break;
    }

    return instr;
}

typedef uint32_t Register;

typedef struct {
    Register r[16];
    
    Register pc;
    Register flags;

    bool running;

    Memory *memory;
} Cpu;

static inline void set_flag(Cpu *cpu, Flag flag, bool v) {
    if (v) cpu->flags |= (1u << flag);
    else cpu->flags &= ~(1u << flag);
}

static inline bool get_flag(Cpu *cpu, Flag flag) {
    return cpu->flags & (1u << flag);
}

static void push32_nocheck(Cpu *cpu, const uint32_t v) {
    cpu->r[SP] -= 4;
    mem_write32(cpu->memory, cpu->r[SP], v);
}

static void raise_exception(Cpu *cpu, uint8_t e) {
    // build the exception frame
    // to avoid recursive exceptions
    // dont check sp bounds and hope
    push32_nocheck(cpu, cpu->flags);
    set_flag(cpu, FLAG_IE, false);
    push32_nocheck(cpu, cpu->pc);
    push32_nocheck(cpu, e);
    push32_nocheck(cpu, mem_read32(cpu->memory, cpu->pc));
    uint32_t target = mem_read32(cpu->memory, IHVT_BASE + ((uint32_t)e * 4));
    cpu->pc = target;
}

[[nodiscard]]
static bool push32(Cpu *cpu, const uint32_t v) {
    if (cpu->r[SP] <= 4) {
        raise_exception(cpu, EX_STACK_OVERFLOW);
        return false;
    }
    cpu->r[SP] -= 4;
    mem_write32(cpu->memory, cpu->r[SP], v);
    return true;
}

[[nodiscard]]
static bool pop32(Cpu *cpu, uint32_t *out) {
    if (cpu->r[SP] >= UINT32_MAX - 4) {
        raise_exception(cpu, EX_STACK_UNDERFLOW);
        return false;
    }
    *out = mem_read32(cpu->memory, cpu->r[SP]);
    cpu->r[SP] += 4;
    return true;
}

static uint32_t compute_jmp_target(Cpu *cpu, Instr d) {
    uint32_t target = (d.is_reg
        ? cpu->r[d.rm]
        : (d.is_signed
            ? (int32_t)((int16_t)d.imm)
            : (uint32_t)d.imm
        )
    );
    target += (d.is_absolute
        ? 0
        : cpu->pc + 4
    );
    return target;
}

static void update_flags_add(Cpu *cpu, uint32_t a, uint32_t b, uint32_t res) {
    uint64_t sum = (uint64_t)a + (uint64_t)b;

    set_flag(cpu, FLAG_Z, res == 0);
    set_flag(cpu, FLAG_N, (res >> 31) & 1u);
    set_flag(cpu, FLAG_C, (sum >> 32) & 1u);
    set_flag(cpu, FLAG_V, ((((~(a ^ b)) & (a ^ res)) >> 31 ) & 1u ));
}

static void update_flags_sub(Cpu *cpu, uint32_t a, uint32_t b, uint32_t res) {
    set_flag(cpu, FLAG_Z, res == 0);
    set_flag(cpu, FLAG_N, (res >> 31) & 1u);
    set_flag(cpu, FLAG_C, a < b);
    set_flag(cpu, FLAG_V, ((((a ^ b) & (a ^ res)) >> 31 ) & 1u ));
}

#define get_rm_or_imm(d) (d.is_reg ? cpu->r[d.rm] : (d.is_signed ? (int32_t)((int16_t)d.imm) : d.imm))

static void step(Cpu *cpu) {
    uint32_t word = mem_read32(cpu->memory, cpu->pc);

    Instr d = decode(word);

    bool update_pc = true;

    switch (d.opcode) {
        case OP_ADD: {
            uint32_t a = cpu->r[d.rn];
            uint32_t b = get_rm_or_imm(d);
            uint32_t res = a + b;
            cpu->r[d.rd] = res;
            update_flags_add(cpu, a, b, res);
            break;
        }

        case OP_SUB: {
            uint32_t a = cpu->r[d.rn];
            uint32_t b = get_rm_or_imm(d);
            uint32_t res = a - b;
            cpu->r[d.rd] = res;
            update_flags_sub(cpu, a, b, res);
            break;
        }

        case OP_XOR: {
            cpu->r[d.rd] = cpu->r[d.rn] ^ get_rm_or_imm(d);
            break;
        }

        case OP_OR: {
            cpu->r[d.rd] = cpu->r[d.rn] | get_rm_or_imm(d);
            break;
        }

        case OP_CMP: {
            uint32_t a = cpu->r[d.rn];
            uint32_t b = get_rm_or_imm(d);
            uint32_t res = a - b;
            update_flags_sub(cpu, a, b, res);
            break;
        }

        case OP_LDR: {
            uint32_t addr = cpu->r[d.rn] + get_rm_or_imm(d);
            cpu->r[d.rd] = mem_read32(cpu->memory, addr);
            break;
        }

        case OP_LDRB: {
            uint32_t addr = cpu->r[d.rn] + get_rm_or_imm(d);
            cpu->r[d.rd] = mem_read8(cpu->memory, addr);
            break;
        }

        case OP_STR: {
            uint32_t addr = cpu->r[d.rn] + get_rm_or_imm(d);
            mem_write32(cpu->memory, addr, cpu->r[d.rd]);
            break;
        }

        case OP_STRB: {
            uint32_t addr = cpu->r[d.rn] + get_rm_or_imm(d);
            mem_write8(cpu->memory, addr, cpu->r[d.rd]);
            break;
        }

        case OP_LUI: {
            cpu->r[d.rd] = (uint32_t)(get_rm_or_imm(d)) << 16;
            break;
        }

        case OP_JXX: {
            uint32_t target = compute_jmp_target(cpu, d);
            bool jump = false;
            switch (d.cond) {
                case COND_JEQ: if (get_flag(cpu, FLAG_Z)) jump = true; break;
                case COND_JNE: if (!get_flag(cpu, FLAG_Z)) jump = true; break;
                case COND_JLT: if (get_flag(cpu, FLAG_N) != get_flag(cpu, FLAG_V)) jump = true; break;
                case COND_JGE: if (get_flag(cpu, FLAG_N) == get_flag(cpu, FLAG_V)) jump = true; break;
                case COND_JLTU: if (!get_flag(cpu, FLAG_C)) jump = true; break;
                case COND_JGEU: if (get_flag(cpu, FLAG_C)) jump = true; break;
                case COND_JCS: if (get_flag(cpu, FLAG_C)) jump = true; break;
                case COND_JCC: if (!get_flag(cpu, FLAG_C)) jump = true; break;
                case COND_JN: if (get_flag(cpu, FLAG_N)) jump = true; break;
                case COND_JP: if (!get_flag(cpu, FLAG_N)) jump = true; break;
                case COND_JVS: if (get_flag(cpu, FLAG_V)) jump = true; break;
                case COND_JVC: if (!get_flag(cpu, FLAG_V)) jump = true; break;
                case COND_JLS: if (!get_flag(cpu, FLAG_C)) jump = true; break;
                default: break;
            }
            if (jump) {
                cpu->pc = target;
                update_pc = false;
            }
            break;
        }

        case OP_CALL: {
            if (!push32(cpu, cpu->pc + 4)) return;
            cpu->pc = compute_jmp_target(cpu, d);
            update_pc = false;
            break;
        }

        case OP_RET: {
            if (!pop32(cpu, &cpu->pc)) return;
            update_pc = false;
            break;
        }

        case OP_PUSH: {
            if (d.is_reg) {
                if (!push32(cpu, cpu->r[d.rm])) return;
            }
            else {
                for (int i = 0; i < 16; i++) {
                    bool push = (cpu->r[d.imm] >> i) & 0x1;
                    if (push) if (!push32(cpu, cpu->r[i])) return;
                }
            }
            break;
        }

        case OP_POP: {
            if (d.is_reg) {
                if (!pop32(cpu, &cpu->r[d.rm])) return;
            }
            else {
                for (int i = 15; i >= 0; i--) {
                    bool pop = (cpu->r[d.imm] >> i) & 0x1;
                    if (pop) if (!pop32(cpu, &cpu->r[i])) return;
                }
            }
            break;
        }

        case OP_HALT: {
            cpu->running = false;
            break;
        }

        default: {
            // TODO: raise_exception(cpu, EX_INVALID_OPCODE);
            printf("INVALID OPCODE 0x%02X\n", d.opcode);
            exit(1);
        }
    }

    if (update_pc) cpu->pc += 4;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s out.bin\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];

    size_t imglen;
    uint8_t *img = load_file(path, &imglen);

    Cpu cpu = {0};
    cpu.r[SP] = UINT32_MAX - sizeof(uint32_t);
    cpu.memory = mem_init();
    for (size_t i = 0; i < imglen; i++) {
        mem_write8(cpu.memory, i, img[i]);
    }
    cpu.running = true;
    while (cpu.running) {
        step(&cpu);
    }
    for (int i = 0; i < 16; i++) {
        printf("R%d: 0x%08X\t", i, cpu.r[i]);
    }
    putc('\n', stdout);    
}