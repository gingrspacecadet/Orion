#ifndef OPS_H
#define OPS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char* name;
    enum {
        I,
        R,
        RI,
        M,
    } type;
    uint8_t opcode;
    size_t num_operands;
} Opcode;

static Opcode opcodes[64] = {
    { "NOP",  R,  0x00, 0},
    { "MOV",  RI, 0x01, 2},
    { "ADD",  RI, 0x02, 3},
    { "SUB",  RI, 0x03, 3},
    { "OR",   RI, 0x05, 3},
    { "SHL",  RI, 0x07, 3},
    { "LDR",  I,  0x0A, 3},
    { "STR",  I,  0x0B, 3},
    { "CMP",  RI, 0x0C, 2},
    { "JMP",  M,  0x0D, 1},
    { "JE",   M,  0x0E, 1},
    { "JNE",  M,  0x0F, 1},
    { "JL",   M,  0x14, 1},
    { "JLE",  M,  0x15, 1},
    { "PUSH", I,  0x16, 1},
    { "POP",  I,  0x17, 1},
    { "HLT",  R,  0x18, 0},
    { "MUL",  RI, 0x1B, 3},
    { "INT",  I,  0x20, 1},
    { "CALL", M,  0x21, 1},
    { "RET",  R,  0x22, 0},
    { "IRET", R,  0x23, 0},
};

#endif