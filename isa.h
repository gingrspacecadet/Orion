#ifndef OPCODES_H
#define OPCODES_H

typedef enum {
    OP_ADD = 0x00,
    OP_SUB = 0x01,
    OP_MUL = 0x02,
    OP_DIV = 0x03,
    OP_SHL = 0x04,
    OP_SHR = 0x05,
    OP_AND = 0x06,
    OP_OR = 0x07,
    OP_NOT = 0x08,
    OP_XOR = 0x09,
    OP_LUI = 0x0A,
    OP_CMP = 0x0B,
    OP_LDR = 0x0C,
    OP_STR = 0x0D,
    OP_LDRB = 0x0E,
    OP_STRB = 0x0F,
    OP_JXX = 0x10,
    OP_CALL = 0x11,
    OP_RET = 0x12,
    OP_PUSH = 0x13,
    OP_POP = 0x14,

    OP_INTE = 0x20,
    OP_FLAGS = 0x21,
    OP_HALT = 0x22,
    OP_ICALL = 0x23,
    OP_IRET = 0x24
} Opcode;

typedef enum {
    COND_JMP  = 0x0,
    COND_JEQ  = 0x1,
    COND_JNE  = 0x2,
    COND_JLT  = 0x3,
    COND_JGE  = 0x4,
    COND_JLTU = 0x5,
    COND_JGEU = 0x6,
    COND_JCS  = 0x7,
    COND_JCC  = 0x8,
    COND_JN   = 0x9,
    COND_JP   = 0xA,
    COND_JVS  = 0xB,
    COND_JVC  = 0xC,
    COND_JLS  = 0xD
} Condition;

typedef enum {
    TYPE_A,
    TYPE_M,
    TYPE_J,
    TYPE_S
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

#endif