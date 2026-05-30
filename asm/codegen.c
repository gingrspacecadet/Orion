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
    FMT_SYS,            // FLAGS rd, imm
    FMT_JUMP,            // JXX target / CALL target
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
    {"ADD", 0x00, TYPE_A, FMT_RD_RN_RM},   {"SUB", 0x01, TYPE_A, FMT_RD_RN_RM},
    {"MUL", 0x02, TYPE_A, FMT_RD_RN_RM},   {"DIV", 0x03, TYPE_A, FMT_RD_RN_RM},
    {"SHL", 0x04, TYPE_A, FMT_RD_RN_RM},   {"SHR", 0x05, TYPE_A, FMT_RD_RN_RM},
    {"AND", 0x06, TYPE_A, FMT_RD_RN_RM},   {"OR",  0x07, TYPE_A, FMT_RD_RN_RM},
    {"NOT", 0x08, TYPE_A, FMT_RD_RN_RM},   {"XOR", 0x09, TYPE_A, FMT_RD_RN_RM},
    {"LUI", 0x0A, TYPE_A, FMT_RD_IMM},     {"CMP", 0x0B, TYPE_A, FMT_RN_IMMRM},
    {"LDR", 0x0C, TYPE_M, FMT_MEM_ACCESS}, {"STR", 0x0D, TYPE_M, FMT_MEM_ACCESS},
    {"LDRB",0x0E, TYPE_M, FMT_MEM_ACCESS}, {"STRB",0x0F, TYPE_M, FMT_MEM_ACCESS},
    {"JXX", 0x10, TYPE_J, FMT_JUMP},       {"CALL",0x11, TYPE_J, FMT_JUMP},
    {"RET", 0x12, TYPE_J, FMT_NONE},       {"PUSH",0x13, TYPE_M, FMT_STACK},
    {"POP", 0x14, TYPE_M, FMT_STACK},      {"FLAGS",0x21, TYPE_S, FMT_SYS},
    {"HALT",0x22, TYPE_X, FMT_NONE},       {"ICALL",0x23, TYPE_J, FMT_JUMP},
    {"IRET",0x24, TYPE_J, FMT_NONE},
    {NULL,0,0,FMT_NONE}
};

static struct { const char *mnem; int val; } condtab[] = {
    {"JMP", 0x0}, {"JEQ", 0x1}, {"JNE", 0x2}, {"JLT", 0x3}, {"JGE", 0x4},
    {"JLTU",0x5}, {"JGEU",0x6}, {"JCS",0x7}, {"JCC",0x8}, {"JN",0x9},
    {"JP",0xA}, {"JVS",0xB}, {"JVC",0xC}, {"JLS",0xD},
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
            *opcode = 0x10; // OP_JXX
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
                write_active_u32(instr.ops[0].val);
                continue;
            }
            if (strcmp(instr.mnemonic, ".byte") == 0) {
                write_active_u8(instr.ops[0].val);
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
                    write_active_u32(encode_a(OP_LUI, 0, dst.reg, false, true, high));
                else
                    write_active_u32(encode_a(OP_XOR, dst.reg, dst.reg, true, false, dst.reg));
                
                if (low != 0)
                    write_active_u32(encode_a(OP_ADD, dst.reg, dst.reg, false, true, low));
                
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
                write_active_u32(encode_a(OP_LUI, 0, dst.reg, false, true, 0));

                reloc_table[reloc_count++] = (ObjReloc){
                    .patch_offset = active_section->ptr,
                    .patch_section = active_section->id,
                    .patch_type = RELOC_LO16,
                    .symbol_name = ""
                };
                strncpy(reloc_table[reloc_count - 1].symbol_name, src.label, 31);
                write_active_u32(encode_a(OP_ADD, dst.reg, dst.reg, false, true, 0));
                continue;
            }

            if (dst.mode == AM_REG && src.mode == AM_MEM) {
                write_active_u32(encode_m(OP_LDR, src.reg, dst.reg, src.is_reg_offset, true, src.val));
                continue;
            }

            if (dst.mode == AM_MEM && src.mode == AM_REG) {
                write_active_u32(encode_m(OP_STR, dst.reg, src.reg, dst.is_reg_offset, true, dst.val));
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
                Operand src2 = instr.ops[2];
                is_reg = (src2.mode == AM_REG);
                immrm = is_reg ? src2.reg : src2.val;

                write_active_u32(encode_a(opcode, rn, rd, is_reg, true, immrm));
                break;
            }

            case FMT_RD_IMM: {
                rd = instr.ops[0].reg;
                rn = 0;
                is_reg = (instr.ops[1].mode == AM_REG);
                immrm = is_reg ? instr.ops[1].reg : instr.ops[1].val;

                write_active_u32(encode_a(opcode, rn, rd, is_reg, true, immrm));
                break;
            }

            case FMT_RN_IMMRM: {
                rd = 0;
                rn = instr.ops[0].reg;
                is_reg = (instr.ops[1].mode == AM_REG);
                immrm = is_reg ? instr.ops[1].reg : instr.ops[1].val;

                write_active_u32(encode_a(opcode, rn, rd, is_reg, true, immrm));
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
                    is_reg = (instr.ops[2].mode == AM_REG);
                    immrm = is_reg ? instr.ops[2].reg : instr.ops[2].val;
                }

                write_active_u32(encode_m(opcode, rn, rd, is_reg, true, immrm));
                break;
            }

            case FMT_STACK: {
                Operand op = instr.ops[0];
                is_reg = (op.mode == AM_REG);
                immrm = is_reg ? op.reg : op.val;
                
                write_active_u32(encode_m(opcode, 0, 0, is_reg, true, immrm));
                break;
            }

            case FMT_SYS: {
                rd = instr.ops[0].reg;
                Operand src = instr.ops[1];
                is_reg = (src.mode == AM_REG);
                immrm = is_reg ? src.reg : src.val;

                write_active_u32(encode_s(opcode, rd, is_reg, true, immrm));
                break;
            }

            case FMT_JUMP: {
                int cond = 0;
                lookup_cond(instr.mnemonic, &cond);
                Operand target = instr.ops[0];

                if (target.mode == AM_LABEL) {
                    reloc_table[reloc_count++] = (ObjReloc){
                        .patch_offset = active_section->ptr,
                        .patch_section = active_section->id,
                        .patch_type = RELOC_PC_REL,
                        .symbol_name = ""
                    };
                    strncpy(reloc_table[reloc_count - 1].symbol_name, target.label, 31);
                    write_active_u32(encode_j(opcode, cond, false, false, true, 0));
                } else {
                    write_active_u32(encode_j(opcode, cond, false, (target.mode == AM_REG), true, target.val));
                }
                break;
            }
        }
    }
}