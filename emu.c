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

typedef struct {
    uint32_t pc;
    uint32_t regs[16];
} Cpu;

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
        uint8_t rm = (word >> 18) & 0xF;

        uint8_t sign = (word >> 1) & 0x1;
        uint8_t ext = word & 0x1;

        int32_t imm;
        if (ext) {
            imm = uint32_tVector_lookup(&boot, ++cpu.pc);
        } else {
            imm = (word >> 2) & 0xFFFF;
        }

        uint8_t reg = 1;
        if (sign || ext) reg = 0;
        else if (rm == 0 && imm != 0) reg = 0;

        switch (opcode) {
            case OP_ADD: {
                if (reg) {
                    cpu.regs[rn] += cpu.regs[rm];
                } else {
                    if (ext && sign) cpu.regs[rn] += imm;
                    else if (ext) cpu.regs[rn] += (uint32_t)imm;
                    else if (sign) cpu.regs[rn] += (int16_t)imm;
                    else cpu.regs[rn] += (uint16_t)imm;
                }
                break;
            }
            case OP_SUB: {
                if (reg) {
                    cpu.regs[rn] -= cpu.regs[rm];
                } else {
                    if (ext && sign) cpu.regs[rn] -= imm;
                    else if (ext) cpu.regs[rn] -= (uint32_t)imm;
                    else if (sign) cpu.regs[rn] -= (int16_t)imm;
                    else cpu.regs[rn] -= (uint16_t)imm;
                }
                break;
            }
            case OP_MOV: {
                if (reg) {
                    cpu.regs[rn] = cpu.regs[rm];
                } else {
                    if (ext && sign) cpu.regs[rn] = imm;
                    else if (ext) cpu.regs[rn] = (uint32_t)imm;
                    else if (sign) cpu.regs[rn] = (int16_t)imm;
                    else cpu.regs[rn] = (uint16_t)imm;
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
}