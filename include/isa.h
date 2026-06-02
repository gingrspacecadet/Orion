#ifndef ISA_H
#define ISA_H

typedef enum {
    OP_ADD   = 0x01,
    OP_SUB   = 0x02,
    OP_MUL   = 0x03,
    OP_DIV   = 0x04,
    OP_SHL   = 0x05,
    OP_SHR   = 0x06,
    OP_AND   = 0x07,
    OP_OR    = 0x08,
    OP_NOT   = 0x09,
    OP_XOR   = 0x0A,
    OP_LUI   = 0x0B,
    OP_CMP   = 0x0C,
    OP_LDR   = 0x0D,
    OP_STR   = 0x0E,
    OP_LDRB  = 0x0F,
    OP_STRB  = 0x10,
    OP_JXX   = 0x11,
    OP_CALL  = 0x12,
    OP_RET   = 0x13,
    OP_PUSH  = 0x14,
    OP_POP   = 0x15,
    OP_INTE  = 0x20,
    OP_FLAGS = 0x21,
    OP_HALT  = 0x22,
    OP_ICALL = 0x23,
    OP_IRET  = 0x24,
} Opcode;

typedef enum {
    COND_JMP,
    COND_JE,
    COND_JNE,
    COND_JLT,
    COND_JGE,
    COND_JLTU,
    COND_JGEU,
    COND_JCS,
    COND_JCC,
    COND_JN,
    COND_JP,
    COND_JVS,
    COND_JVC,
    COND_JLS
} Condition;

typedef enum {
    TYPE_INVALID,
    TYPE_A,
    TYPE_M,
    TYPE_J,
    TYPE_S,
    TYPE_X
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
    FLAG_C,
    FLAG_V,
    FLAG_Z,
    FLAG_N,
    FLAG_IE,
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

#endif