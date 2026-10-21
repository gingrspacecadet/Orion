#ifndef ISA_H
#define ISA_H

#define X_OPCODES \
    X(NOP) \
    X(SUB) \
    X(ADD) \
    X(MUL) \
    X(DIV) \
    X(DIVU) \
    X(SHL) \
    X(SHR) \
    X(ASHR) \
    X(AND) \
    X(OR) \
    X(XOR) \
    X(LUI) \
    X(LDR) \
    X(STR) \
    X(LDRB) \
    X(STRB) \
    X(JMP) \
    X(JMPR) \
    X(JXX) \
    X(CALL) \
    X(CALLR) \
    X(RET) \
    X(FADD) \
    X(FMUL) \
    X(FDIV) \
    X(FCMP) \
    X(ITOF) \
    X(FTOI) \
    X(XCHG) \
    X(FENCE) \
    X(RPC) \
    X(FLAGS) \
    X(HALT) \
    X(SYSCALL) \
    X(IRET) \

#define X(name) OP_##name,
typedef enum {
    X_OPCODES
} Opcodes;
#undef X

#ifdef X_OPCODES_IMPL
#define X(NAME) #NAME,
const char *const opcode_names[] = {
    X_OPCODES
};
#undef X
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
#define J_DISPLACEMENT(word)    ((word)         & 0x7FFFFFF)
#define I_RN(word)              (((word) >> 22) & 0xF)
#define B_COND(word)            (((word) >> 23) & 0x7)
#define B_RN(word)              (((word) >> 19) & 0xF)
#define B_OPERAND(word)         (((word) >> 14) & 0x1F)
#define B_DISPLACEMENT(word)    (((word) >> 1)  & 0x1FFF)
#define A_RN(word)              (((word) >> 22) & 0xF)
#define A_RD(word)              (((word) >> 18) & 0xF)
#define A_OPERAND(word)         (((word) >> 2)  & 0xFFFF)
#define A_REGISTER(word)        (((word) >> 1)  & 0x1)
#define M_RN(word)              A_RN(word)
#define M_RD(word)              M_RD(word)
#define M_OPERAND(word)         A_OPERAND(word)
#define M_REGISTER(word)        A_REGISTER(word)
#define S_REG(word)             (((word) >> 22) & 0xF)
#define S_PAYLOAD(word)         (((word) >> 6)  & 0xFFFF)
#define S_REGISTER(word)        (((word) >> 1)  & 0x1)
#define S_WRITE(worD)           ((word)         & 0x1)

#endif