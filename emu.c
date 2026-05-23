#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "isa.h"

typedef struct {
    uint8_t opcode;
    uint8_t rn;
    uint8_t rd;
    bool is_reg;
    bool is_signed;
    uint16_t imm;
    uint8_t rm;

    // J-type
    uint8_t cond;
    bool is_absolute;

    // FLAGS
    bool is_write;
} Instr;

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

static int opcode_type(uint8_t opcode) {
    switch (opcode) {
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_SHL:
        case OP_SHR:
        case OP_AND:
        case OP_OR:
        case OP_NOT:
        case OP_XOR:
        case OP_LUI:
        case OP_CMP: return TYPE_A;

        case OP_LDR:
        case OP_STR:
        case OP_LDRB:
        case OP_STRB:
        case OP_PUSH:
        case OP_POP: return TYPE_M;

        case OP_JXX:
        case OP_CALL:
        case OP_RET:
        case OP_ICALL:
        case OP_IRET: return TYPE_J;

        case OP_FLAGS: return TYPE_S;

        default: return -1;
    }
}

static Instr decode(uint32_t word) {
    Instr instr = {0};
    instr.opcode = (word >> 26) & 0x3F;
    
    // same with all opcodes
    instr.is_reg = (word >> IS_REG_SHIFT) & IS_REG_MASK;
    instr.is_signed = (word >> SIGNED_SHIFT) & SIGNED_MASK;

    if (instr.is_reg) {
        instr.rm = (word >> IMM_RM_SHIFT) & RM_MASK;
    } else {
        instr.imm = (word >> IMM_RM_SHIFT) & IMM_MASK;
    }

    switch (opcode_type(instr.opcode)) {
        case TYPE_M:
        case TYPE_A: {
            instr.rn = (word >> RN_SHIFT) & RN_MASK;
            instr.rd = (word >> RD_SHIFT) & RD_MASK;
            break;
        }

        case TYPE_J: {
            instr.cond = (word >> COND_SHIFT) & COND_MASK;
            instr.is_absolute = (word >> ABS_SHIFT) & ABS_MASK;
            // TODO: raise exception if `reserved` non-zero
            break;
        }

        case TYPE_S: {
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
        }

        default: break;
    }

    return instr;
}

static uint32_t load_le32(const uint8_t b[4]) {
    return ((uint32_t)b[0]) |
           ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s out.bin\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];

    size_t imglen;
    uint8_t *img = load_file(path, &imglen);

    Instr i = decode(load_le32(img));
    printf("OPC=0x%02X\n", i.opcode);
    printf("0x%02X R%d, R%d, R%d\n", i.opcode, i.rd, i.rn, i.imm);
}