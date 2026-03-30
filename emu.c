#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "vector.h"
#include "opcodes.h"

VECTOR_DECLARE(uint32_t);

uint32_tVector open_file(char *path) {
    if (!path) exit(1);

    FILE *fptr = fopen(path, "r");
    if (!fptr) {
        fprintf(stderr, "Failed to open file %s\n", path);
        exit(1);
    }
    fseek(fptr, 0, SEEK_END);
    size_t flen = ftell(fptr);
    fseek(fptr, 0, SEEK_SET);
    char *fbuf = malloc(flen);
    if (!fbuf) {
        fprintf(stderr, "Failed to allocate file buffer\n");
        exit(1);
    }
    if (fread(fbuf, 1, flen, fptr) != flen) {
        fprintf(stderr, "Failed to read file %s\n", path);
        exit(1);
    }
    fclose(fptr);

    uint32_tVector s = uint32_tVector_init();
    uint32_tVector_resize(&s, flen);
    memcpy(s.data, fbuf, flen);
    s.cap = (flen / 4) + (flen % 4 != 0);
    s.idx = 0;

    return s;
}

#define flag_set(flags, flag) ((flags) |= (1 << (flag)))
#define flag_clear(flags, flag) ((flags) &= ~(1 << (flag)))
#define flag_get(flags, flag) (((flags) >> flag) & 0x1)

#define FLAG_CARRY          0
#define FLAG_OVERFLOW       1
#define FLAG_ZERO           2
#define FLAG_NEGATIVE       3
#define FLAG_INT_ENABLED    4

typedef struct {
    uint32_t pc;
    uint32_t regs[16];
    uint8_t flags;
} Cpu;

void print_regs(Cpu *cpu) {
    for (int i = 0; i < 8; i++) {
        printf("R%d 0x%08X\t", i, cpu->regs[i]);
    }
    putc('\n', stdout);
    for (int i = 8; i < 16; i++) {
        printf("R%d 0x%08X\t", i, cpu->regs[i]);
    }
    putc('\n', stdout);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Missing target boot image\n");
        exit(1);
    }

    uint32_tVector boot = open_file(argv[1]);

    Cpu cpu = {0};

    while (cpu.pc < boot.cap) {
        uint32_t word = uint32_tVector_lookup(&boot, cpu.pc);
        uint8_t opcode = (word >> 26) & 0x3F;

        uint8_t rn = (word >> 22) & 0xF;
        uint8_t rd = (word >> 18) & 0xF;
        uint8_t rm = (word >> 14) & 0xF;

        uint8_t reg = (word >> 1) & 0x1;
        uint8_t ext = word & 0x1;

        uint32_t imm = 0;
        if (!reg) {
            if (ext) {
                imm = uint32_tVector_lookup(&boot, ++cpu.pc);
            } else {
                imm = (word >> 2) & 0xFFFF;
            }
        }

        switch (opcode) {
            case OP_ADD: {
                cpu.regs[rd] = cpu.regs[rn] + imm + (reg ? cpu.regs[rm] : 0);
                break;
            }
            case OP_SUB: {
                cpu.regs[rd] = cpu.regs[rn] - imm - (reg ? cpu.regs[rm] : 0);
                break;
            }
            case OP_MOV: {
                if (reg) {
                    cpu.regs[rn] = cpu.regs[rm];
                } else {
                    cpu.regs[rn] = imm;
                }
                break;
            }
            case OP_JXX: {
                uint8_t cond = (word >> 22) & 0xF;
                uint8_t should = 0;
                switch (cond) {
                    case 0x0: should = 1; break;
                    case 0x1: should = flag_get(cpu.flags, FLAG_ZERO) == 1; break;
                    case 0x2: should = flag_get(cpu.flags, FLAG_ZERO) == 0; break;
                    case 0x3: should = flag_get(cpu.flags, FLAG_NEGATIVE) != flag_get(cpu.flags, FLAG_OVERFLOW); break;
                    case 0x4: should = flag_get(cpu.flags, FLAG_NEGATIVE) == flag_get(cpu.flags, FLAG_OVERFLOW); break;
                    case 0x5: should = flag_get(cpu.flags, FLAG_CARRY) == 0; break;
                    case 0x6: should = flag_get(cpu.flags, FLAG_CARRY) == 1; break;
                    case 0x7: should = flag_get(cpu.flags, FLAG_CARRY) == 1; break;
                    case 0x8: should = flag_get(cpu.flags, FLAG_CARRY) == 0; break;
                    case 0x9: should = flag_get(cpu.flags, FLAG_NEGATIVE) == 1; break;
                    case 0xA: should = flag_get(cpu.flags, FLAG_NEGATIVE) == 0; break;
                    case 0xB: should = flag_get(cpu.flags, FLAG_OVERFLOW) == 1; break;
                    case 0xC: should = flag_get(cpu.flags, FLAG_OVERFLOW) == 0; break;
                    case 0xD: should = (flag_get(cpu.flags, FLAG_CARRY) == 1) && (flag_get(cpu.flags, FLAG_ZERO) == 0); break;
                    case 0xE: should = flag_get(cpu.flags, FLAG_CARRY) == 0; break;
                    case 0xF: // TODO: cpu excpetions!
                }
                if (!should) break;
                uint8_t absolute = (word >> 2) & 0x1;
                if (absolute) {
                    if (reg) {
                        cpu.pc = cpu.regs[rm];
                    } else {
                        cpu.pc = imm;
                    }
                } else {
                    if (reg) {
                        cpu.pc = (int32_t)cpu.pc + (int32_t)cpu.regs[rm];
                    } else {
                        cpu.pc = (int32_t)cpu.pc + (int32_t)imm;
                    }
                }

                break;
            }

            default: {
                fprintf(stderr, "Invalid opcode %d\n", opcode);
                exit(1);    // TODO: interrupts!!
            }
        }

        cpu.pc++;
    }

    print_regs(&cpu);

    return 0;
}