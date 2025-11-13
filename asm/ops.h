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
    { .name = "NOP", .type = R,  .opcode = 0b00000000, .num_operands = 0},
    { .name = "ADD", .type = RI, .opcode = 0b00001000, .num_operands = 3},
    { .name = "SUB", .type = RI, .opcode = 0b00001100, .num_operands = 3},
    { .name = "MOV", .type = RI, .opcode = 0b00000100, .num_operands = 2},
    { .name = "HLT", .type = R,  .opcode = 0b01011100, .num_operands = 0},
    { .name = "JMP", .type = M,  .opcode = 0b00110100, .num_operands = 1},
    { .name = "INT", .type = I,  .opcode = 0b01111100, .num_operands = 1},
    { .name = "CALL",.type = M,  .opcode = 0b10000000, .num_operands = 1},
    { .name = "RET", .type = R,  .opcode = 0b10000100, .num_operands = 0},
};

#endif