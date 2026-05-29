#include <stdint.h>
#include "parser.h"
#include "isa.h"
#include "obj.h"

#define MAX_SECTION_SIZE    65536
#define MAX_SYMBOLS         1024
#define MAX_RELOCS          1024

uint8_t text_section[MAX_SECTION_SIZE];
uint32_t text_ptr = 0;

uint8_t data_section[MAX_SECTION_SIZE];
uint32_t data_ptr = 0;

ObjSymbol symbol_table[MAX_SYMBOLS];
int symbol_count = 0;

ObjReloc reloc_table[MAX_RELOCS];
int reloc_count = 0;
uint32_t pc = 0;

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
    // reserved bits left zero
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

static void write_text_u32(uint32_t word) {
    if (text_ptr + 4 > MAX_SECTION_SIZE) {
        fprintf(stderr, "Assembler Error: .text section overflow.\n");
        exit(1);
    }
    text_section[text_ptr++] = (word >>  0) & 0xFF;
    text_section[text_ptr++] = (word >>  8) & 0xFF;
    text_section[text_ptr++] = (word >> 16) & 0xFF;
    text_section[text_ptr++] = (word >> 24) & 0xFF;
}

typedef struct {
    const char *mnem;
    uint32_t code;
    int type;
} OpInfo;

static OpInfo optab[] = {
    {"ADD", 0x00, TYPE_A}, {"SUB", 0x01, TYPE_A}, {"MUL", 0x02, TYPE_A}, {"DIV", 0x03, TYPE_A},
    {"SHL", 0x04, TYPE_A}, {"SHR", 0x05, TYPE_A}, {"AND", 0x06, TYPE_A}, {"OR", 0x07, TYPE_A},
    {"NOT", 0x08, TYPE_A}, {"XOR", 0x09, TYPE_A}, {"LUI", 0x0A, TYPE_A}, {"CMP", 0x0B, TYPE_A},
    {"LDR", 0x0C, TYPE_M}, {"STR", 0x0D, TYPE_M}, {"LDRB",0x0E, TYPE_M}, {"STRB",0x0F, TYPE_M},
    {"JXX", 0x10, TYPE_J}, {"CALL",0x11, TYPE_J}, {"RET",0x12, TYPE_J}, {"PUSH",0x13, TYPE_M},
    {"POP", 0x14, TYPE_M}, {"FLAGS",0x21, TYPE_S}, {"HALT",0x22, TYPE_X}, {"ICALL",0x23, TYPE_J},
    {"IRET",0x24, TYPE_J},
    // condition mnemonics are handled separately (JEQ, JNE, etc.)
    {NULL,0,0}
};

static struct { const char *mnem; int val; } condtab[] = {
    {"JMP", 0x0}, {"JEQ", 0x1}, {"JNE", 0x2}, {"JLT", 0x3}, {"JGE", 0x4},
    {"JLTU",0x5}, {"JGEU",0x6}, {"JCS",0x7}, {"JCC",0x8}, {"JN",0x9},
    {"JP",0xA}, {"JVS",0xB}, {"JVC",0xC}, {"JLS",0xD},
    {NULL, -1}
};

static int lookup_opcode(const char *mnem, uint32_t *opcode, int *type) {
    for (int i = 0; optab[i].mnem; i++) {
        if (strcasecmp(optab[i].mnem, mnem) == 0) {
            *opcode = optab[i].code;
            *type = optab[i].type;
            return 1;
        }
    }
    for (int i = 0; condtab[i].mnem; i++) {
        if (strcasecmp(condtab[i].mnem, mnem) == 0) {
            *opcode = 0x10;
            *type = TYPE_J;
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
                .offset = text_ptr,
                .section = 1,
                .is_global = true,
                .name = "",
            };
            strncpy(symbol_table[symbol_count - 1].name, instr.mnemonic, 31);
            continue;
        }

        if (instr.is_directive) {
            if (strcmp(instr.mnemonic, ".word") == 0) {
                uint8_t *v = (uint8_t *)&instr.ops[0].val;
                data_section[data_ptr++] = v[0];
                data_section[data_ptr++] = v[1];
                data_section[data_ptr++] = v[2];
                data_section[data_ptr++] = v[3];
                continue;
            }
            if (strcmp(instr.mnemonic, ".byte") == 0) {
                data_section[data_ptr++] = (uint8_t)instr.ops[0].val;
                continue;
            }
            if (strcmp(instr.mnemonic, ".org") == 0) {
                text_ptr = instr.ops[0].val;
                continue;
            }
            
            fprintf(stderr, "Assembler Error (Line %d): Unknown directive '%s'.\n", instr.line_num, instr.mnemonic);
            exit(1);
        }

        if (strcasecmp(instr.mnemonic, "mov") == 0) {
            Operand dst = instr.ops[0];
            Operand src = instr.ops[1];

            if (dst.mode == AM_REG && src.mode == AM_REG) {
                write_text_u32(encode_a(OP_OR, src.reg, dst.reg, true, false, src.reg));
                continue;
            }

            if (dst.mode == AM_REG && src.mode == AM_IMM) {
                uint32_t high = (uint32_t)((uint64_t)src.val >> 16) & 0xFFFF;
                uint32_t low = (uint32_t)src.val & 0xFFFF;

                // micro-optimisation: use `xor` as it is cheaper than `lui`
                if (high != 0)
                    write_text_u32(encode_a(OP_LUI, 0, dst.reg, false, true, high));
                else
                    write_text_u32(encode_a(OP_XOR, dst.reg, dst.reg, true, false, dst.reg));
                
                if (low != 0)
                    write_text_u32(encode_a(OP_ADD, dst.reg, dst.reg, false, true, low));
                
                continue;
            }

            if (dst.mode == AM_REG && src.mode == AM_LABEL) {
                reloc_table[reloc_count++] = (ObjReloc){
                    .patch_offset = text_ptr,
                    .patch_type = RELOC_HI16,
                    .symbol_name = ""
                };
                strncpy(reloc_table[reloc_count - 1].symbol_name, src.label, 31);
                write_text_u32(encode_a(OP_LUI, 0, dst.reg, false, true, 0));

                reloc_table[reloc_count++] = (ObjReloc){
                    .patch_offset = text_ptr,
                    .patch_type = RELOC_LO16,
                    .symbol_name = ""
                };
                strncpy(reloc_table[reloc_count - 1].symbol_name, src.label, 31);
                write_text_u32(encode_a(OP_ADD, dst.reg, dst.reg, false, true, 0));
                continue;
            }

            if (dst.mode == AM_REG && src.mode == AM_MEM) {
                write_text_u32(encode_m(OP_LDR, src.reg, dst.reg, src.is_reg_offset, true, src.val));
                continue;
            }

            if (dst.mode == AM_MEM && src.mode == AM_REG) {
                write_text_u32(encode_m(OP_STR, dst.reg, src.reg, dst.is_reg_offset, true, dst.val));
                continue;
            }

            fprintf(stderr, "Assembler Error (Line %d): Invalid mov operands.\n", instr.line_num);
            exit(1);
        }

        // just a plain instruction
        uint32_t opcode;
        int type;
        if (!lookup_opcode(instr.mnemonic, &opcode, &type)) {
            fprintf(stderr, "Assembler Error (Line %d): Unknown instruction '%s'\n", instr.line_num, instr.mnemonic);
            exit(1);
        }

        if (type == TYPE_J) {
            int cond = 0;
            lookup_cond(instr.mnemonic, &cond);


            Operand target = instr.ops[0];

            if (target.mode == AM_LABEL) {
                reloc_table[reloc_count++] = (ObjReloc){
                    .patch_offset = text_ptr,
                    .patch_type = RELOC_PC_REL,
                };
                strncpy(reloc_table[reloc_count - 1].symbol_name, target.label, 31);

                write_text_u32(encode_j(opcode, cond, false, false, true, 0));
            } else {
                write_text_u32(encode_j(opcode, cond, false, (target.mode == AM_REG), true, target.val));
            }
            continue;
        }

        if (type == TYPE_A || type == TYPE_M) {
            int rn = instr.ops[1].reg;
            int rd = instr.ops[0].reg;
            Operand src2 = instr.ops[2];
            int is_reg = (src2.mode == AM_REG);
            int64_t immrm = is_reg ? src2.reg : src2.val;

            write_text_u32(encode_a(opcode, rn, rd, is_reg, true, immrm));
            continue;
        }

        if (type == TYPE_S) {
            int rd = instr.ops[0].reg;
            Operand src = instr.ops[1];
            int is_reg = (src.mode == AM_REG);
            int64_t immrm = is_reg ? src.reg : src.val;
            write_text_u32(encode_s(opcode, rd, is_reg, true, immrm));
            continue;
        }

        if (type == TYPE_X) {
            write_text_u32((opcode &0x3F) << 26);
            continue;
        }

        fprintf(stderr, "Assembler Error (Line %d): Unknown encoding for instruction '%s'\n", instr.line_num, instr.mnemonic);
        exit(1);
    }
}