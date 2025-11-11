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
    { .name = "NOP", .type = R,  .opcode = (uint8_t)0b00000000, .num_operands = 0},
    { .name = "MOV", .type = RI, .opcode = (uint8_t)0b00000100, .num_operands = 2},
    { .name = "HALT",.type = R,  .opcode = (uint8_t)0b01011100, .num_operands = 0},
    { .name = "B",   .type = M,  .opcode = (uint8_t)0b00110100, .num_operands = 1}
};

#endif