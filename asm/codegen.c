#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "parser.h"
#include "isa.h"
#include "obj.h"
#include "codegen.h"

Section sections[MAX_SECTIONS];
int section_count = 0;
Section *active_section = NULL;

void switch_section(const char *name) {
    for (int i = 0; i < section_count; i++) {
        if (strcmp(sections[i].name, name) == 0) {
            active_section = &sections[i];
            return;
        }
    }

    if (section_count >= MAX_SECTIONS) {
        fprintf(stderr, "Assembler Error: Exceeded maximum allowed sections (%d)\n", MAX_SECTIONS);
        exit(1);
    }

    Section *new_sec = &sections[section_count++];
    memset(new_sec, 0, sizeof(Section));
    strncpy(new_sec->name, name, 31);
    new_sec->id = section_count;
    active_section = new_sec;
    printf("Assembler: Created new section '%s' (ID: %d)\n", name, section_count);
}

#define MAX_SYMBOLS         1024
#define MAX_RELOCS          1024

ObjSymbol symbol_table[MAX_SYMBOLS];
int symbol_count = 0;

ObjReloc reloc_table[MAX_RELOCS];
int reloc_count = 0;
uint32_t pc = 0;

typedef enum {
    FMT_NONE,           // HALT, RET, IRET
    FMT_RD_RN_RM,       // ADD rd, rn, rm  / SUB rd, rn, imm
    FMT_RD_IMM,         // LUI rd, imm
    FMT_RN_IMMRM,       // CMP rn, rm/imm
    FMT_MEM_ACCESS,     // LDR rd, [rn + offset] / STR rd, [rn + offset]
    FMT_STACK,          // PUSH / POP handling
    FMT_JUMP,           // JXX target / CALL target
} OpFormat;

static uint32_t encode_a(uint32_t opcode, int rn, int rd, bool is_reg, bool is_signed, int64_t immrm) {
    uint32_t w = 0;
    w |= (opcode & 0x3F) << 26;
    w |= (rn & 0xF) << 22;
    w |= (rd & 0xF) << 18;
    if (is_reg) {
        uint32_t rm = (uint32_t)immrm & 0xF;
        w |= (rm & 0xF) << 2;
    } else {
        uint32_t imm = (uint32_t)immrm & 0xFFFF;
        w |= (imm & 0xFFFF) << 2;
    }
    if (is_reg) w |= 1u << 1;
    if (is_signed) w |= 1u << 0;
    return w;
}

static uint32_t encode_m(uint32_t opcode, int rn, int rd, bool is_reg, bool is_signed, int64_t immrm) {
    return encode_a(opcode, rn, rd, is_reg, is_signed, immrm);
}

static uint32_t encode_j(uint32_t opcode, int cond, bool absolute, bool is_reg, bool signed_flag, int64_t immrm) {
    uint32_t w = 0;
    w |= (opcode & 0x3F) << 26;
    w |= (cond & 0xF) << 22;
    if (absolute) w |= 1u << 21;
    if (is_reg) {
        uint32_t rm = (uint32_t)immrm & 0xF;
        w |= (rm & 0xF) << 2;
    } else {
        uint32_t imm16 = (uint32_t)immrm & 0xFFFF;
        w |= (imm16 & 0xFFFF) << 2;
    }
    if (is_reg) w |= 1u << 1;
    if (signed_flag) w |= 1u << 0;
    return w;
}

static uint32_t encode_s(uint32_t opcode, int rd, bool is_reg, bool rw, int64_t immrm) {
    uint32_t w = 0;
    w |= (opcode & 0x3F) << 26;
    w |= (rd & 0xF) << 22;
    if (is_reg) {
        uint32_t rm = (uint32_t)immrm & 0xF;
        w |= (rm & 0xF) << 6;
    } else {
        uint32_t imm16 = (uint32_t)immrm & 0xFFFF;
        w |= (imm16 & 0xFFFF) << 6;
    }
    if (is_reg) w |= 1u << 1;
    if (rw) w |= 1u << 0;
    return w;
}

static void write_active_u32(uint32_t word) {
    if (!active_section) {
        switch_section(".text");
    }

    if (active_section->ptr + 4 > MAX_SECTION_SIZE) {
        fprintf(stderr, "Assembler Error: Section '%s' overflow\n", active_section->name);
        exit(1);
    }

    active_section->buffer[active_section->ptr++] = (word >>  0) & 0xFF;
    active_section->buffer[active_section->ptr++] = (word >>  8) & 0xFF;
    active_section->buffer[active_section->ptr++] = (word >> 16) & 0xFF;
    active_section->buffer[active_section->ptr++] = (word >> 24) & 0xFF;
}

static void write_active_u8(uint8_t word) {
    if (!active_section) {
        switch_section(".text");
    }

    if (active_section->ptr + 4 > MAX_SECTION_SIZE) {
        fprintf(stderr, "Assembler Error: Section '%s' overflow\n", active_section->name);
        exit(1);
    }

    active_section->buffer[active_section->ptr++] = word;
}

typedef struct {
    const char *mnem;
    uint32_t code;
    int type;
    OpFormat format;
} OpInfo;

static OpInfo optab[] = {
    {"ADD",  OP_ADD,  TYPE_A, FMT_RD_RN_RM},   {"SUB", OP_SUB,   TYPE_A, FMT_RD_RN_RM},
    {"MUL",  OP_MUL,  TYPE_A, FMT_RD_RN_RM},   {"DIV", OP_DIV,   TYPE_A, FMT_RD_RN_RM},
    {"SHL",  OP_SHL,  TYPE_A, FMT_RD_RN_RM},   {"SHR", OP_SHR,   TYPE_A, FMT_RD_RN_RM},
    {"AND",  OP_AND,  TYPE_A, FMT_RD_RN_RM},   {"OR",  OP_OR,    TYPE_A, FMT_RD_RN_RM},
    {"NOT",  OP_NOT,  TYPE_A, FMT_RD_RN_RM},   {"XOR", OP_XOR,   TYPE_A, FMT_RD_RN_RM},
    {"LUI",  OP_LUI,  TYPE_A, FMT_RD_IMM},     {"CMP", OP_CMP,   TYPE_A, FMT_RN_IMMRM},
    {"LDR",  OP_LDR,  TYPE_M, FMT_MEM_ACCESS}, {"STR", OP_STR,   TYPE_M, FMT_MEM_ACCESS},
    {"LDRB", OP_LDRB, TYPE_M, FMT_MEM_ACCESS}, {"STRB",OP_STRB,  TYPE_M, FMT_MEM_ACCESS},
    {"JXX",  OP_JXX,  TYPE_J, FMT_JUMP},       {"CALL",OP_CALL,  TYPE_J, FMT_JUMP},
    {"RET",  OP_RET,  TYPE_J, FMT_NONE},       {"PUSH",OP_PUSH,  TYPE_M, FMT_STACK},
    {"POP",  OP_POP,  TYPE_M, FMT_STACK},      {"HALT", OP_HALT, TYPE_X, FMT_NONE},
    {"ICALL",OP_ICALL,TYPE_J, FMT_JUMP},       {"IRET", OP_IRET, TYPE_J, FMT_NONE},
    {NULL,0,0,FMT_NONE}
};

static struct { const char *mnem; int val; } condtab[] = {
    {"JMP",  COND_JMP}, 
    {"JEQ",  COND_JEQ},  {"JNE",  COND_JNE}, 
    {"JLT",  COND_JLT},  {"JGE",  COND_JGE},
    {"JLTU", COND_JLTU}, {"JGEU", COND_JGEU}, 
    {"JCS",  COND_JCS},  {"JCC",  COND_JCC}, 
    {"JN",   COND_JN},   {"JP",   COND_JP},
    {"JVS",  COND_JVS},  {"JVC",  COND_JVC}, 
    {"JLS",  COND_JLS},
    {NULL, -1}
};

static int lookup_opcode(const char *mnem, uint32_t *opcode, int *type, OpFormat *format) {
    for (int i = 0; optab[i].mnem; i++) {
        if (strcasecmp(optab[i].mnem, mnem) == 0) {
            *opcode = optab[i].code;
            *type = optab[i].type;
            *format = optab[i].format;
            return 1;
        }
    }
    for (int i = 0; condtab[i].mnem; i++) {
        if (strcasecmp(condtab[i].mnem, mnem) == 0) {
            *opcode = OP_JXX;
            *type = TYPE_J;
            *format = FMT_JUMP;
            return 1;
        }
    }
    return 0;
}

static int lookup_cond(const char *mnem, int *cond) {
    for (int i = 0; condtab[i].mnem; ++i) {
        if (strcasecmp(condtab[i].mnem, mnem) == 0) {
            *cond = condtab[i].val;
            return 1;
        }
    }
    return 0;
}

static int64_t encode_operand(Operand op, int patch_type, bool *is_reg) {
    if (op.mode == AM_LABEL) {
        if (reloc_count >= MAX_RELOCS) {
            fprintf(stderr, "Assembler Error: Maximum relocations exceeded.\n");
            exit(1);
        }
        reloc_table[reloc_count++] = (ObjReloc){
            .patch_offset = active_section->ptr,
            .patch_section = active_section->id,
            .patch_type = patch_type,
            .symbol_name = ""
        };
        strncpy(reloc_table[reloc_count - 1].symbol_name, op.label, 31);

        if (is_reg) *is_reg = false;
        return 0;
    }

    if (is_reg) *is_reg = op.mode == AM_REG;
    return (op.mode == AM_REG) ? op.reg : op.val;
}

#define SHOULD_SIGN_EXTEND(val) ((val) >= -32768 && (val) <= 32767)

void codegen(instr_array *instrs) {
    for (size_t i = 0; i < instrs->len; i++) {
        ParsedInstr instr = instrs->data[i];

        if (instr.is_label_def) {
            symbol_table[symbol_count++] = (ObjSymbol){
                .offset = active_section->ptr,
                .section = active_section->id,
                .is_global = true,
                .name = "",
            };
            strncpy(symbol_table[symbol_count - 1].name, instr.mnemonic, 31);
            continue;
        }

        if (instr.is_directive) {
            if (strcmp(instr.mnemonic, ".word") == 0) {
                write_active_u32(encode_operand(instr.ops[0], RELOC_32, NULL));
                continue;
            }
            if (strcmp(instr.mnemonic, ".byte") == 0) {
                write_active_u8(encode_operand(instr.ops[0], RELOC_32, NULL));
                continue;
            }
            if (strcmp(instr.mnemonic, ".section") == 0) {
                switch_section(instr.ops[0].label);
                continue;
            }
            
            fprintf(stderr, "Assembler Error (Line %d): Unknown directive '%s'.\n", instr.line_num, instr.mnemonic);
            exit(1);
        }

        // Handle pseudoinstruction 'MOV'
        if (strcasecmp(instr.mnemonic, "mov") == 0) {
            Operand dst = instr.ops[0];
            Operand src = instr.ops[1];

            if (dst.mode == AM_REG && src.mode == AM_REG) {
                write_active_u32(encode_a(OP_OR, src.reg, dst.reg, true, false, src.reg));
                continue;
            }

            if (dst.mode == AM_REG && src.mode == AM_IMM) {
                uint32_t high = (uint32_t)((uint64_t)src.val >> 16) & 0xFFFF;
                uint32_t low = (uint32_t)src.val & 0xFFFF;

                if (high != 0)
                    write_active_u32(encode_a(OP_LUI, 0, dst.reg, false, SHOULD_SIGN_EXTEND(high), high));
                else
                    write_active_u32(encode_a(OP_XOR, dst.reg, dst.reg, true, false, dst.reg));
                
                if (low != 0)
                    write_active_u32(encode_a(OP_ADD, dst.reg, dst.reg, false, SHOULD_SIGN_EXTEND(low), low));
                
                continue;
            }

            if (dst.mode == AM_REG && src.mode == AM_LABEL) {
                reloc_table[reloc_count++] = (ObjReloc){
                    .patch_offset = active_section->ptr,
                    .patch_section = active_section->id,
                    .patch_type = RELOC_HI16,
                    .symbol_name = ""
                };
                strncpy(reloc_table[reloc_count - 1].symbol_name, src.label, 31);
                write_active_u32(encode_a(OP_LUI, 0, dst.reg, false, false, 0));

                reloc_table[reloc_count++] = (ObjReloc){
                    .patch_offset = active_section->ptr,
                    .patch_section = active_section->id,
                    .patch_type = RELOC_LO16,
                    .symbol_name = ""
                };
                strncpy(reloc_table[reloc_count - 1].symbol_name, src.label, 31);
                write_active_u32(encode_a(OP_ADD, dst.reg, dst.reg, false, false, 0));
                continue;
            }

            if (dst.mode == AM_REG && src.mode == AM_MEM) {
                write_active_u32(encode_m(OP_LDR, src.reg, dst.reg, src.is_reg_offset, SHOULD_SIGN_EXTEND(src.val), src.val));
                continue;
            }

            if (dst.mode == AM_MEM && src.mode == AM_REG) {
                write_active_u32(encode_m(OP_STR, dst.reg, src.reg, dst.is_reg_offset, SHOULD_SIGN_EXTEND(dst.val), dst.val));
                continue;
            }

            // `flags` operations
            if (dst.mode == AM_REG && src.mode == AM_LABEL && strcasecmp(src.label, "flags") == 0) {
                write_active_u32(encode_s(OP_FLAGS, dst.reg, false, false, 0));
                continue;
            }

            if (dst.mode == AM_LABEL && strcasecmp(dst.label, "flags") == 0 && src.mode == AM_REG) {
                write_active_u32(encode_s(OP_FLAGS, 0, true, true, src.reg));
                continue;
            }

            if (dst.mode == AM_LABEL && strcasecmp(dst.label, "flags") == 0 && src.mode == AM_IMM) {
                write_active_u32(encode_s(OP_FLAGS, 0, false, true, src.val));
                continue;
            }

            fprintf(stderr, "Assembler Error (Line %d): Invalid mov operands.\n", instr.line_num);
            exit(1);
        }

        uint32_t opcode;
        int type;
        OpFormat format;
        if (!lookup_opcode(instr.mnemonic, &opcode, &type, &format)) {
            fprintf(stderr, "Assembler Error (Line %d): Unknown instruction '%s'\n", instr.line_num, instr.mnemonic);
            exit(1);
        }

        int rn = 0;
        int rd = 0;
        bool is_reg = false;
        int64_t immrm = 0;

        switch (format) {
            case FMT_NONE: {
                if (type == TYPE_X) {
                    write_active_u32((opcode & 0x3F) << 26);
                } else if (type == TYPE_J) {
                    write_active_u32(encode_j(opcode, 0, false, false, false, 0));
                }
                break;
            }

            case FMT_RD_RN_RM: {
                rd = instr.ops[0].reg;
                rn = instr.ops[1].reg;
                immrm = encode_operand(instr.ops[2], RELOC_LO16, &is_reg);

                write_active_u32(encode_a(opcode, rn, rd, is_reg, SHOULD_SIGN_EXTEND(immrm), immrm));
                break;
            }

            case FMT_RD_IMM: {
                rd = instr.ops[0].reg;
                rn = 0;
                immrm = encode_operand(instr.ops[1], (opcode == OP_LUI) ? RELOC_HI16 : RELOC_LO16, &is_reg);

                write_active_u32(encode_a(opcode, rn, rd, is_reg, SHOULD_SIGN_EXTEND(immrm), immrm));
                break;
            }

            case FMT_RN_IMMRM: {
                rd = 0;
                rn = instr.ops[0].reg;
                immrm = encode_operand(instr.ops[1], RELOC_LO16, &is_reg);

                write_active_u32(encode_a(opcode, rn, rd, is_reg, SHOULD_SIGN_EXTEND(immrm), immrm));
                break;
            }

            case FMT_MEM_ACCESS: {
                if (instr.ops[0].mode == AM_REG && instr.ops[1].mode == AM_MEM) {
                    rd = instr.ops[0].reg;
                    rn = instr.ops[1].reg; 
                    is_reg = instr.ops[1].is_reg_offset;
                    immrm = instr.ops[1].val;
                } else if (instr.ops[0].mode == AM_MEM && instr.ops[1].mode == AM_REG) {
                    rd = instr.ops[1].reg;
                    rn = instr.ops[0].reg;
                    is_reg = instr.ops[0].is_reg_offset;
                    immrm = instr.ops[0].val;
                } else {
                    rd = instr.ops[0].reg;
                    rn = instr.ops[1].reg;
                    immrm = encode_operand(instr.ops[2], RELOC_LO16, &is_reg);
                }

                write_active_u32(encode_m(opcode, rn, rd, is_reg, SHOULD_SIGN_EXTEND(immrm), immrm));
                break;
            }

            case FMT_STACK: {
                immrm = encode_operand(instr.ops[0], RELOC_LO16, &is_reg);
                
                write_active_u32(encode_m(opcode, 0, 0, is_reg, SHOULD_SIGN_EXTEND(immrm), immrm));
                break;
            }

            case FMT_JUMP: {
                int cond = 0;
                lookup_cond(instr.mnemonic, &cond);
                immrm = encode_operand(instr.ops[0], RELOC_PC_REL, &is_reg);

                write_active_u32(encode_j(opcode, cond, false, is_reg, SHOULD_SIGN_EXTEND(immrm), immrm));
                break;
            }
        }
    }
}