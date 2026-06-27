#ifndef ISA_H
#define ISA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#ifndef DEBUG
    #define INLINE __always_inline
#else
    #define INLINE
#endif

typedef enum {
    OP_NOP   = 0x00,
    OP_SUB   = 0x01,
    OP_ADD   = 0x02,
    OP_MUL   = 0x03,
    OP_DIV   = 0x04,
    OP_DIVU  = 0x05,
    OP_SHL   = 0x06,
    OP_SHR   = 0x07,
    OP_SHRU  = 0x08,
    OP_AND   = 0x09,
    OP_OR    = 0x0A,
    OP_XOR   = 0x0B,
    OP_LUI   = 0x0C,
    OP_LDR   = 0x0D,
    OP_STR   = 0x0E,
    OP_LDRB  = 0x0F,
    OP_STRB  = 0x10,
    OP_JMP   = 0x11,
    OP_JMPA  = 0x12,
    OP_JXX   = 0x13,
    OP_CALL  = 0x14,
    OP_CALLA = 0x15,
    OP_RET   = 0x16,
    OP_FADD  = 0x17,
    OP_FSUB  = 0x18,
    OP_FMUL  = 0x19,
    OP_FDIV  = 0x1A,
    OP_FCMP  = 0x1B,
    OP_ITOF  = 0x1C,
    OP_FTOI  = 0x1D,
    OP_XCHG  = 0x1E,
    OP_FENCE = 0x1F,
    OP_RPC   = 0x20,
    OP_FLAGS = 0x21,
    OP_HALT  = 0x22,
   OP_SYSCALL= 0x23,
    OP_IRET  = 0x24,
} Opcode;

typedef enum {
    COND_JEQ = 1,
    COND_JNE,
    COND_JLT,
    COND_JGE,
    COND_JLTU,
    COND_JGEU
} Condition;

typedef enum {
    TYPE_INVALID,
    TYPE_J,
    TYPE_B,
    TYPE_A,
    TYPE_M,
    TYPE_S,
    TYPE_X,
} OpcodeType;

#define OPCODE_SHIFT    36
#define OPCODE_MASK     0x3F

#define RN_SHIFT        22
#define RN_MASK         0xF

#define RD_SHIFT        18
#define RD_MASK         0xF

#define IMM_RM_SHIFT    2
#define IMM_MASK        0xFFFF
#define RM_MASK         0xF

#define IS_REG_SHIFT    1
#define IS_REG_MASK     0x1

#define SIGNED_SHIFT    0
#define SIGNED_MASK     0x1

#define COND_SHIFT      22
#define COND_MASK       0xF
#define ABS_SHIFT       21
#define ABS_MASK        0x1

#define SP              15

typedef enum {
    FLAG_IE,
    FLAG_PRV,
    FLAG_VM,
    FLAG_TM,
} Flag;

typedef enum {
    EX_INVALID_INSTR,
    EX_MISALIGNED_PC,
    EX_INVALID_MEM_ACCESS,
    EX_STACK_UNDERFLOW,
    EX_STACK_OVERFLOW,
    EX_NON_MASKABLE_INT = 0xFF,
} Exception;

#define IHVT_BASE           0x00010000

typedef struct Instr {
    uint8_t opcode;
    uint8_t rn;
    uint8_t rd;
    uint8_t rm;
    uint8_t cond;
    bool is_reg;
    bool is_signed;
    bool is_absolute;
    bool is_write;
    uint32_t imm;
} Instr;

static INLINE const char *opcode_name(uint8_t op) {
    switch (op) {
        case OP_NOP:    return "nop";
        case OP_ADD:    return "add";
        case OP_SUB:    return "sub";
        case OP_MUL:    return "mul";
        case OP_DIV:    return "div";
        case OP_DIVU:   return "divu";
        case OP_SHL:    return "shl";
        case OP_SHR:    return "shr";
        case OP_SHRU:   return "shru";
        case OP_AND:    return "and";
        case OP_OR:     return "or";
        case OP_XOR:    return "xor";
        case OP_LUI:    return "lui";
        case OP_LDR:    return "ldr";
        case OP_LDRB:   return "ldrb";
        case OP_STR:    return "str";
        case OP_STRB:   return "strb";
        case OP_JMP:    return "jmp";
        case OP_JMPA:   return "jmpa";
        case OP_JXX:    return "jxx";
        case OP_CALL:   return "call";
        case OP_CALLA:  return "calla";
        case OP_RET:    return "ret";
        case OP_FADD:   return "fadd";
        case OP_FSUB:   return "fsub";
        case OP_FMUL:   return "fmul";
        case OP_FDIV:   return "fdiv";
        case OP_FCMP:   return "fcmp";
        case OP_ITOF:   return "itof";
        case OP_FTOI:   return "ftoi";
        case OP_XCHG:   return "xchg";
        case OP_FENCE:  return "fence";
        case OP_RPC:    return "rpc";
        case OP_FLAGS:  return "flags";
        case OP_HALT:   return "halt";
        case OP_SYSCALL:return "syscall";
        case OP_IRET:   return "iret";
        default:        return "data";
    }
}

static INLINE const char *cond_name(uint8_t cond) {
    switch (cond) {
        case COND_JEQ:  return "eq";
        case COND_JNE:  return "ne";
        case COND_JLT:  return "lt";
        case COND_JGE:  return "ge";
        case COND_JLTU: return "ltu";
        case COND_JGEU: return "geu";
        default:        return "??";
    }
}

static INLINE int opcode_type(uint8_t opcode) {
    switch (opcode) {
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_DIVU:
        case OP_SHL:
        case OP_SHR:
        case OP_SHRU:
        case OP_AND:
        case OP_OR:
        case OP_XOR:
        case OP_LUI:
            return TYPE_A;

        case OP_LDR:
        case OP_STR:
        case OP_LDRB:
        case OP_STRB:
            return TYPE_M;
            
        case OP_JXX:
            return TYPE_B;

        case OP_JMP:
        case OP_JMPA:
        case OP_CALL:
        case OP_CALLA:
        case OP_RET:
        case OP_IRET:
            return TYPE_J;

        case OP_FLAGS:
            return TYPE_S;

        default:
            return -1;
    }
}

static INLINE Instr isa_decode(uint32_t word) {
    Instr instr = {0};

    instr.opcode = (word >> 26) & 0x3F;
    instr.is_reg = (word >> IS_REG_SHIFT) & IS_REG_MASK;
    instr.is_signed = (word >> SIGNED_SHIFT) & SIGNED_MASK;

    if (instr.is_reg) {
        instr.rm = (word >> IMM_RM_SHIFT) & RM_MASK;
    } else {
        instr.imm = (word >> IMM_RM_SHIFT) & IMM_MASK;
    }

    switch (opcode_type(instr.opcode)) {
        case TYPE_A:
        case TYPE_M:
            instr.rn = (word >> RN_SHIFT) & RN_MASK;
            instr.rd = (word >> RD_SHIFT) & RD_MASK;
            break;

        case TYPE_J:
            instr.imm = (word) & 0x4000000;
            break;

        case TYPE_B:
            instr.cond = (word >> COND_SHIFT) & COND_MASK;
            instr.rn = (word >> (COND_SHIFT - 4)) & RN_MASK;
            instr.rd = (word >> (COND_SHIFT - 8)) & RD_MASK;
            instr.imm = (word >> 2) & 0xFFF;
            instr.is_absolute = (word) & 0x1;
            break;

        case TYPE_S:
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

        default:
            break;
    }

    return instr;
}

static INLINE size_t isa_disassemble(char *dst, size_t cap, uint32_t pc, uint32_t word) {
    Instr d = isa_decode(word);

    if (d.opcode == OP_HALT || d.opcode == OP_RET || d.opcode == OP_IRET || d.opcode == OP_NOP) {
        return (size_t)snprintf(dst, cap, "%s", opcode_name(d.opcode));
    }

    if (d.opcode == OP_JXX) {
        uint32_t target = d.is_absolute
            ? ((uint32_t)d.imm << 2)
            : (pc + (uint32_t)((int16_t)((uint32_t)d.imm << 2)));

        return (size_t)snprintf(dst, cap,
                                "j%s r%u, %c%u, 0x%08" PRIX32,
                                cond_name(d.cond), d.rn, d.is_reg, d.rd, target);
    }

    if (d.opcode == OP_FLAGS) {
        if (d.is_write) {
            if (d.is_reg) {
                return (size_t)snprintf(dst, cap, "flags wr r%u", d.rm);
            }
            return (size_t)snprintf(dst, cap, "flags wr 0x%04" PRIX16, d.imm);
        }
        return (size_t)snprintf(dst, cap, "flags r%u", d.rd);
    }

    if (opcode_type(d.opcode) == TYPE_A || opcode_type(d.opcode) == TYPE_M) {
        if (d.is_reg) {
            return (size_t)snprintf(dst, cap, "%s r%u, r%u, r%u",
                                    opcode_name(d.opcode), d.rd, d.rn, d.rm);
        }
        if (d.is_signed) {
            return (size_t)snprintf(dst, cap, "%s r%u, r%u, %" PRId16,
                                    opcode_name(d.opcode), d.rd, d.rn, (int16_t)d.imm);
        }
        return (size_t)snprintf(dst, cap, "%s r%u, r%u, %" PRIu16,
                                opcode_name(d.opcode), d.rd, d.rn, d.imm);
    }

    return (size_t)snprintf(dst, cap, "%s", opcode_name(d.opcode));
}

#endif