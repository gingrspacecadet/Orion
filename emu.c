#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

typedef enum {
    OP_ADD = 0x00,
    OP_SUB = 0x01,
    OP_MUL = 0x02,
    OP_DIV = 0x03,
    OP_SHL = 0x04,
    OP_SHR = 0x05,
    OP_AND = 0x06,
    OP_OR = 0x07,
    OP_NOT = 0x08,
    OP_XOR = 0x09,
    OP_LUI = 0x0A,
    OP_CMP = 0x0B,
    OP_LDR = 0x0C,
    OP_STR = 0x0D,
    OP_LDRB = 0x0E,
    OP_STRB = 0x0F,
    OP_JXX = 0x10,
    OP_CALL = 0x11,
    OP_RET = 0x12,
    OP_PUSH = 0x13,
    OP_POP = 0x14,

    OP_INTE = 0x20,
    OP_FLAGS = 0x21,
    OP_HALT = 0x22,
    OP_ICALL = 0x23,
    OP_IRET = 0x24
} Opcode;

typedef enum {
    COND_JMP  = 0x0,
    COND_JEQ  = 0x1,
    COND_JNE  = 0x2,
    COND_JLT  = 0x3,
    COND_JGE  = 0x4,
    COND_JLTU = 0x5,
    COND_JGEU = 0x6,
    COND_JCS  = 0x7,
    COND_JCC  = 0x8,
    COND_JN   = 0x9,
    COND_JP   = 0xA,
    COND_JVS  = 0xB,
    COND_JVC  = 0xC,
    COND_JLS  = 0xD
} Condition;

typedef enum {
    EXC_INVALID_INSTR = 0x00,
    EXC_MISALIGNED_PC = 0x01,
    EXC_INVALID_MEM   = 0x02,
    EXC_STACK_FAULT   = 0x03,
    EXC_NMI           = 0xFF
} Exception;

typedef struct {
    uint8_t opcode;
    uint8_t rn, rd, rm;
    uint16_t imm;
    uint8_t cond;
    bool is_absolute, is_register, is_signed, is_write, is_enabled;
} DecodedInstr;

typedef struct {
    uint32_t r[16];
    uint32_t pc;
    uint32_t flags;

    bool is_running;
    uint8_t *memory;
} Cpu;

DecodedInstr decode(uint32_t raw) {
    DecodedInstr instr = {0};

    instr.opcode = (raw >> 26) & 0x3F;

    // these are literally always in the same place
    instr.is_signed = (raw >> 0) & 0x01;
    instr.is_register = (raw >> 1) & 0x01;

    if (instr.is_register) {
        instr.rm = (raw >> 14) & 0x0F;

        uint16_t reserved_check = (raw >> 2) & 0x0FFF;
        if (reserved_check) {
            instr.opcode = 0xFF;
        }
    } else {
        instr.imm = (raw >> 2) & 0xFFFF;
    }

    if (instr.opcode <= OP_CMP || (instr.opcode >= OP_LDR && instr.opcode <= OP_POP)) {
        instr.rn = (raw >> 22) & 0x0F;
        instr.rd = (raw >> 18) & 0x0F;
        
    } else if (instr.opcode == OP_JXX || instr.opcode == OP_CALL || instr.opcode == OP_RET) {
        instr.cond = (raw >> 22) & 0x0F;
        instr.is_absolute = (raw >> 21) & 0x01;
        
        uint8_t j_reserved = (raw >> 18) & 0x07;
        if (j_reserved != 0) instr.opcode = 0xFF;
        
    } else if (instr.opcode == OP_FLAGS) {
        instr.is_write = (raw >> 0) & 0x01;
        
        if (!instr.is_write) {
            instr.rd = (raw >> 22) & 0x0F;
        }
    } else if (instr.opcode == OP_INTE) {
        instr.is_enabled = (raw >> 25) & 0x01;
    }

    return instr;
}

int main(void) {
    Cpu cpu = {0};
    cpu.memory = (uint8_t*)calloc(1, 0x1000);

    cpu.is_running = true;

    while (cpu.is_running) {
        // TODO: interrupts

        uint32_t raw = cpu.memory[cpu.pc]; cpu.pc += 4;
        if (!cpu.is_running) break; // tmp

        DecodedInstr instr = decode(raw);

        uint32_t op2 = instr.is_register ? cpu.r[instr.rm] : instr.imm;

        switch (instr.opcode) {
            case OP_ADD: {
                cpu.r[instr.rd] = cpu.r[instr.rn] + op2;
                // TODO: flags
                break;
            }

            case OP_SUB:
            case OP_CMP: {
                uint32_t result = cpu.r[instr.rn] - op2;
                if (instr.opcode == OP_SUB) cpu.r[instr.rd] = result;
                // flags
                break;
            }

            case OP_LUI: {
                cpu.r[instr.rd] = (op2 & 0xFFFF) << 16;
                break;
            }

            default: {
                printf("FAULT: unknown opcode %d\n", instr.opcode);
                return 1;
            }
        }
    }
}