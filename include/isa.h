#ifndef ISA_H
#define ISA_H

#include <stddef.h>
#include <stdarg.h>

#define X_OPCODES \
    X(NOP, TYPE_X) \
    X(SUB, TYPE_A) \
    X(ADD, TYPE_A) \
    X(MUL, TYPE_A) \
    X(DIV, TYPE_A) \
    X(DIVU, TYPE_A) \
    X(SHL, TYPE_A) \
    X(SHR, TYPE_A) \
    X(ASHR, TYPE_A) \
    X(AND, TYPE_A) \
    X(OR, TYPE_A) \
    X(XOR, TYPE_A) \
    X(LUI, TYPE_A) \
    X(LDR, TYPE_M) \
    X(STR, TYPE_M) \
    X(LDRB, TYPE_M) \
    X(STRB, TYPE_M) \
    X(JMP, TYPE_J) \
    X(JMPR, TYPE_I) \
    X(JXX, TYPE_B) \
    X(CALL, TYPE_J) \
    X(CALLR, TYPE_I) \
    X(RET, TYPE_X) \
    X(FADD, TYPE_A) \
    X(FSUB, TYPE_A) \
    X(FMUL, TYPE_A) \
    X(FDIV, TYPE_A) \
    X(FCMP, TYPE_A) \
    X(ITOF, TYPE_A) \
    X(FTOI, TYPE_A) \
    X(XCHG, TYPE_M) \
    X(FENCE, TYPE_X) \
    X(RPC, TYPE_S) \
    X(FLAGS, TYPE_S) \
    X(HALT, TYPE_X) \
    X(SYSCALL, TYPE_X) \
    X(IRET, TYPE_X) \
    X(BRKPT, TYPE_X) \
    X(END, TYPE_X) \

#define X(name, type) OP_##name,
typedef enum {
    X_OPCODES
} Opcodes;
#undef X

#ifdef X_OPCODES_IMPL
#define X(name, type) #name,
const char *const opcode_names[] = {
    X_OPCODES
};
#undef X
#else
extern const char *const opcode_names[];
#endif

typedef enum {
    COND_JEQ = 1,
    COND_JNE,
    COND_JLT,
    COND_JGE,
    COND_JLTU,
    COND_JGEU,
} Conditions;

typedef enum {
    TYPE_J = 1,
    TYPE_I,
    TYPE_B,
    TYPE_A,
    TYPE_M,
    TYPE_S,
    TYPE_X,
} OpcodeTypes;

#define OPCODE(word)            (((word) >> 26) & 0x7F) 
#define J_DISPLACEMENT(word)    ((word)         & 0x3FFFFFF)
#define I_RN(word)              (((word) >> 22) & 0xF)
#define B_COND(word)            (((word) >> 23) & 0x7)
#define B_RN(word)              (((word) >> 19) & 0xF)
#define B_OPERAND(word)         (((word) >> 14) & 0x1F)
#define B_DISPLACEMENT(word)    (((word) >> 1)  & 0x1FFF)
#define B_REGISTER(word)        ((word)         & 0x1)
#define A_RN(word)              (((word) >> 22) & 0xF)
#define A_RD(word)              (((word) >> 18) & 0xF)
#define A_OPERAND(word)         (((word) >> 2)  & 0xFFFF)
#define A_REGISTER(word)        (((word) >> 1)  & 0x1)
#define M_RN(word)              A_RN(word)
#define M_RD(word)              A_RD(word)
#define M_OPERAND(word)         A_OPERAND(word)
#define M_REGISTER(word)        A_REGISTER(word)
#define S_REG(word)             (((word) >> 22) & 0xF)
#define S_PAYLOAD(word)         (((word) >> 6)  & 0xFFFF)
#define S_REGISTER(word)        (((word) >> 1)  & 0x1)
#define S_WRITE(word)           ((word)         & 0x1)

#define BOOT 0x00001000
#define IHVT 0x00010000
#define ICU  0x00010400
#define PIT  0x00010500
#define MTU  0x00010600
#define PCU  0x00010800
#define RAM  0x00020000

#define INVALID_INSTR   0x00
#define MISALIGNED_PC   0x01
#define INVALID_MEM     0x02
#define STACK_UNDERFLOW 0x03
#define STACK_OVERFLOW  0x04
#define ITLB_MISS       0x05
#define DTLB_MISS       0x06
#define PAGE_FAULT      0x07
#define SYSCALL         0x08
#define ARITHMETIC_FAULT  0x09
#define MISALIGNED_DATA 0x0A
#define PRIV_VIOLATION  0x0B
#define NMI             0xFF

#define ICU_IRR     0x00
#define ICU_ISR     0x04
#define ICU_IMR     0x08
#define ICU_PRIO    0x0C
#define ICU_VEC     0x2C
#define ICU_EOI     0x4C
#define ICU_DEFAULT 0x50

#define PIT_CTRL    0x00
#define PIT_INT_LO  0x04
#define PIT_INT_HI  0x08
#define PIT_CUR_LO  0x0C
#define PIT_CUR_HI  0x10
#define PIT_ACK     0x14

#define PIT_CTRL_ENABLE 1u << 0
#define PIT_CTRL_AUTO   1u << 1
#define PIT_CTRL_INT    1u << 2

#define PCU_DEV_ID      0x00
#define PCU_DEV_BAR     0x04
#define PCU_DEV_SIZE    0x08
#define PCU_DEV_IRQ     0x0C

#define MTU_FAULT_ADDR      0x00
#define MTU_CONFIG          0x04
#define TLB_HI_WIN          0x08
#define TLB_LO_WIN          0x0C
#define MTU_CURR_ASID       0x10
#define MTU_FAULT_STATUS    0x14

#endif