#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <stdint.h>

#include "source.h"

typedef enum {
    AM_NONE,
    AM_REG,
    AM_IMM,
    AM_REF,
    AM_MEM,
    AM_REGLIST,
    AM_IDENT,
} AddrMode;

typedef struct {
    AddrMode mode;
    int reg;
    int offset_reg;
    bool is_reg_offset;
    int64_t val;
    char label[32];
} Operand;

typedef struct {
    bool is_label;
    bool is_directive;
    bool is_eof;
    char name[32];
    Operand *ops;
    uint8_t op_count;
    int line_num;
} Instr;

Operand parse_operand(Source *source);
Instr parse(Source *source);

#endif