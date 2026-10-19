#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

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

static inline uint32_t read_u32le(const uint8_t *p, size_t offset) {
    uint32_t x;
    memcpy(&x, p + offset, sizeof(x));

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    x = __builtin_bswap32(x);
#endif

    return x;
}

typedef uint32_t Register;

typedef struct {
    Register usr[16];
    Register ksr[16];
        #define RSP     15
    Register flags;
        #define FLAG_IE  0
        #define FLAG_PRV 1
        #define FLAG_VM  2
        #define FLAG_TM  3
    Register pc;
} Cpu;

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

#define J_DISPLACEMENT(word)    ((word)         & 0x7FFFFFF)
#define I_RN(word)              (word)   >> 22) & 0xF)
#define B_COND(word)            (((word) >> 23) & 0x7)
#define B_RN(word)              (word)   >> 19) & 0xF)
#define B_OPERAND(word)         (((word) >> 14) & 0x1F)
#define B_DISPLACEMENT(word)    (((word) >> 1)  & 0x1FFF)
#define A_RN(word)              (word)   >> 22) & 0xF)
#define A_RD(word)              (word)   >> 18) & 0xF)
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

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s image.bin\n", argv[0]);
        return 1;
    }

    size_t imgsz;
    uint8_t *img = load_file(argv[1], &imgsz);

    return 0;
}