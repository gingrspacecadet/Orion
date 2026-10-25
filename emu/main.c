#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "isa.h"

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
    Register *r;
        #define RSP     15
    Register flags;
        #define FLAG_IE  0
        #define FLAG_PRV 1
        #define FLAG_VM  2
        #define FLAG_TM  3
    Register pc;
} Cpu;

typedef struct {
    Cpu cpu;
    uint8_t *mem;
} Machine;

void cpu_reset(Cpu *cpu) {
    cpu->pc = 0x1000;
    cpu->flags = 0x2;
    cpu->r = cpu->ksr;
}

#define R(N) machine->cpu.r[N]

static inline int32_t sign_extend_16(uint32_t value) {
    value &= 0xFFFFu;
    return (value & 0x8000u)
        ? (int32_t)value - 0x10000
        : (int32_t)value;
}

static inline int32_t sign_extend_26(uint32_t value) {
    value &= 0x3FFFFFF;
    return (value & 0x100000)
        ? (int32_t)value - 0x200000
        : (int32_t)value;
}

static inline int32_t sign_extend_13(uint32_t value) {
    value &= 0x1FFFFFF;
    return (value & 0x80000)
        ? (int32_t)value - 0x100000
        : (int32_t)value;
}

static inline int64_t signed32(uint32_t value) {
    return (value & 0x80000000u)
        ? (int64_t)value - 0x100000000LL
        : (int64_t)value;
}

static inline uint32_t arithmetic_rhs(Machine *machine, uint32_t word) {
    if (A_REGISTER(word))
        return R(A_OPERAND(word) & 0xF);

    return (uint32_t)sign_extend_16(A_OPERAND(word));
}

void step(Machine *machine) {
    uint32_t word = read_u32le(machine->mem, machine->cpu.pc);

    bool advance = true;

    switch (OPCODE(word)) {
        case OP_ADD: {
            R(A_RD(word)) = R(A_RN(word)) + arithmetic_rhs(machine, word);
            break;
        }

        case OP_SUB: {
            R(A_RD(word)) = R(A_RN(word)) - arithmetic_rhs(machine, word);
            break;
        }

        case OP_MUL: {
            R(A_RD(word)) = R(A_RN(word)) * arithmetic_rhs(machine, word);
            break;
        }

        case OP_DIV: {
            int64_t lhs = signed32(R(A_RN(word)));
            int64_t rhs = A_REGISTER(word) ? signed32(R(A_OPERAND(word) & 0xF)) : sign_extend_16(A_OPERAND(word));

            if (rhs == 0) {
                fprintf(stderr, "Arithmetic fault: signed division by zero\n");
                exit(EXIT_FAILURE);
            }

            if (lhs == INT32_MIN && rhs == -1) {
                fprintf(stderr, "Arithmetic fault: signed division overflow\n");
                exit(EXIT_FAILURE);
            }

            R(A_RD(word)) = (uint32_t)(lhs / rhs);
            break;
        }

        case OP_DIVU: {
            uint32_t lhs = R(A_RN(word));
            uint32_t rhs = A_REGISTER(word) ? R(A_OPERAND(word) & 0xF) : A_OPERAND(word);

            if (rhs == 0) {
                fprintf(stderr, "Arithmetic fault: unsigned division by zero\n");
                exit(EXIT_FAILURE);
            }

            R(A_RD(word)) = lhs / rhs;
            break;
        }

        case OP_JMP: {
            int32_t target = machine->cpu.pc + 4 + sign_extend_26(J_DISPLACEMENT(word)) * 4;
            if (target % 4 != 0) {
                fprintf(stderr, "Misaligned PC fault\n");
                exit(1);
            }

            machine->cpu.pc = target;
            advance = false;
            break;
        }

        case OP_JXX: {
            int32_t target = machine->cpu.pc + 4 + sign_extend_13(B_DISPLACEMENT(word)) * 4;
            if (target % 4 != 0) {
                fprintf(stderr, "Misaligned PC fault\n");
                exit(1);
            }

            uint32_t operand = B_REGISTER(word) ? (B_OPERAND(word) & 0xF) : B_OPERAND(word);

            bool jump = false;
            switch (B_COND(word)) {
                case COND_JEQ:  if (B_RN(word) == operand) jump = true; break;
                case COND_JGE:  if ((int32_t)B_RN(word) >= (int32_t)operand) jump = true; break;
                case COND_JGEU: if (B_RN(word) >= operand) jump = true; break;
                case COND_JLT:  if ((int32_t)B_RN(word)  < (int32_t)operand) jump = true; break;
                case COND_JLTU: if (B_RN(word) < operand) jump = true; break;
                case COND_JNE:  if (B_RN(word) != operand) jump = true; break;
                default: {
                    fprintf(stderr, "Unknown jump condition\n");
                    exit(1);
                }
            }

            break;
        }

        case OP_NOP: {
            break;
        }

        default: {
            fprintf(stderr, "Unknown opcode 0x%X!\n", OPCODE(word));
            exit(1);
        }
    }

    if (advance) machine->cpu.pc += 4;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s image.bin\n", argv[0]);
        return 1;
    }

    size_t imgsz;
    uint8_t *img = load_file(argv[1], &imgsz);

    Machine m = {
        .mem = mmap(NULL, 1ull << 32, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0),
    };

    memcpy(m.mem + 0x1000, img, imgsz);
    cpu_reset(&m.cpu);

    for (size_t i = 0; i < 1000000; i++) {
        step(&m);
    }

    printf("reg 0 = %d\n", m.cpu.r[0]);

    return 0;
}