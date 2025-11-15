#ifndef OPS_H
#define OPS_H

#include <stddef.h>

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
    { "NOP",  R,  0b00000000, 0},
    { "MOV",  RI, 0b00000100, 2},
    { "ADD",  RI, 0b00001000, 3},
    { "SUB",  RI, 0b00001100, 3},
    { "OR",   RI, 0b00010100, 3},
    { "SHL",  RI, 0b00011100, 3},
    { "LDR",  I,  0b00101000, 3},
    { "STR",  I,  0b00101100, 3},
    { "JMP",  M,  0b00110100, 1},
    { "PUSH", I,  0b01010100, 1},
    { "POP",  I,  0b01011000, 1},
    { "HLT",  R,  0b01011100, 0},
    { "INT",  I,  0b01111100, 1},
    { "CALL", M,  0b10000000, 1},
    { "RET",  R,  0b10000100, 0},
};

#endif