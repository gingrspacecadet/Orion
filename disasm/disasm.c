#include "isa.h"
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

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
            break;
        }

        default: break;
    }

    return instr;
}

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

static uint32_t load_le32(const uint8_t b[4]) {
    return ((uint32_t)b[0]) |
           ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s out.bin\n", argv[0]);
        return 1;
    }

    size_t imglen;
    uint8_t *img = load_file(argv[1], &imglen);

    for (size_t i = 0; i < imglen; i += 4) {
        uint32_t raw = load_le32(&img[i]);

        if (raw == 0) {
            size_t zeros = 0;
            while (i + 4 <= imglen && load_le32(&img[i]) == 0) {
                zeros++;
                i += 4;
            }
            printf("(%zu zeros)\n", zeros);
            continue;
        }

        Instr d = decode(load_le32((uint8_t*)&raw));

        switch (d.opcode) {
            case OP_ADD: printf("add"); break;
            case OP_SUB: printf("sub"); break;
            case OP_MUL: printf("mul"); break;
            case OP_DIV: printf("div"); break;
            case OP_SHL: printf("shl"); break;
            case OP_SHR: printf("shr"); break;
            case OP_AND: printf("and"); break;
            case OP_OR: printf("or"); break;
            case OP_NOT: printf("not"); break;
            case OP_XOR: printf("xor"); break;
            case OP_LUI: printf("lui"); break;
            case OP_CMP: printf("cmp"); break;
            case OP_LDR: printf("ldr"); break;
            case OP_LDRB: printf("ldrb"); break;
            case OP_STR: printf("str"); break;
            case OP_STRB: printf("strb"); break;
            case OP_PUSH: printf("push"); break;
            case OP_POP: printf("pop"); break;
            case OP_JXX: {
                printf("j");
                switch (d.cond) {
                    case COND_JEQ: printf("eq"); break;
                    case COND_JNE: printf("ne"); break;
                    case COND_JLT: printf("lt"); break;
                    case COND_JGE: printf("ge"); break;
                    case COND_JLTU: printf("ltu"); break;
                    case COND_JGEU: printf("geu"); break;
                    case COND_JCS: printf("cs"); break;
                    case COND_JCC: printf("cc"); break;
                    case COND_JN: printf("n"); break;
                    case COND_JP: printf("p"); break;
                    case COND_JVS: printf("vs"); break;
                    case COND_JVC: printf("vc"); break;
                    case COND_JLS: printf("ls"); break;
                    default: printf("mp"); break;
                }
                break;
            }
            case OP_CALL: printf("call"); break;
            case OP_RET: printf("ret"); break;
            case OP_ICALL: printf("icall"); break;
            case OP_IRET: printf("iret"); break;
            case OP_FLAGS: printf("flags"); break;
            case OP_HALT: printf("halt"); break;
            default: printf("<unknown>"); break;
        }

        if (d.opcode == OP_PUSH || d.opcode == OP_POP) {
            if (d.is_reg) {
                printf(" r%d", d.rm);
            } else {
                if (d.is_signed) {
                    printf(" %" PRId16, d.imm);
                } else {
                    printf(" %" PRIu16, d.imm);
                }
            }
        } else {
            printf(" r%d, ", d.rd);
            printf("r%d, ", d.rn);
            if (d.is_reg) {
                printf("r%d", d.rm);
            } else {
                if (d.is_signed) {
                    printf("%" PRId16, d.imm);
                } else {
                    printf("%" PRIu16, d.imm);
                }
            }
        }
        
        printf("\n");
    }
}