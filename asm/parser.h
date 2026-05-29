#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "gin.h"

typedef enum {
    AM_NONE,
    AM_REG,
    AM_IMM,
    AM_LABEL,
    AM_MEM,
    AM_REGLIST,
} AddrMode;

typedef struct {
    AddrMode mode;
    int reg;
    int offset_reg;
    int is_reg_offset;
    int64_t val;
    char label[32];
} Operand;

typedef struct {
    int is_label_def;
    int is_directive;
    char mnemonic[32];
    Operand ops[3];
    int op_count;
    int line_num;
} ParsedInstr;

INSTANTIATE(ParsedInstr, instr, ARRAY_TEMPLATE)

instr_array parse(token_array *tokens);

#endif