#ifndef ISA_H
#define ISA_H

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

#endif