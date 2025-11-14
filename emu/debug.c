#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "machine.h"
#include "debug.h"
#include "../asm/ops.h"

/* Find table entry by opcode byte; returns pointer or NULL */
static const Opcode* dlookup(uint8_t opcode) {
    for (size_t i = 0; i < sizeof(opcodes)/sizeof(opcodes[0]); ++i) {
        if (opcodes[i].opcode == opcode) return &opcodes[i];
    }
    return NULL;
}

/* Utility getters used by disassembler (mirror assembler layout) */
static inline uint8_t instr_opcode(uint32_t instr) {
    /* opcode stored in highest byte (as assembler left-shifted by 24) */
    return (uint8_t)(instr >> 24);
}

/* For R / RI style: assume layout (from assembler):
   bits: [31:24] opcode (6 meaningful bits but in a whole byte),
   then destination reg and source reg/imm in the high bits.
   We'll decode following the assembler bit placements used:
   - For R (two regs): dest at bits 26..23 (4 bits), src at bits 22..19 (4 bits)
   - For RI: dest in same place, immediate placed lower (we infer width)
   - For M (branch): immediate offset occupies lower 23 bits (as assembler shifted by (32-6-23))
   These choices reproduce the assembler packing used earlier.
*/

/* Disassemble a single 32-bit instruction into human readable text */
static void disasm(uint32_t instr, char *out, size_t outlen, uint32_t pc) {
    if (!out || outlen == 0) return;
    out[0] = '\0';

    /* Normalise opcode: assembler put opcode in the top byte (we clear lower 2 bits) */
    uint8_t opc = (instr_opcode(instr) >> 2) << 2;
    const Opcode *entry = dlookup(opc);
    if (!entry) {
        snprintf(out, outlen, "db 0x%02X", (uint32_t)opc);
        return;
    }

    switch (entry->type) {
    case R: {
        uint32_t dest = getbits(instr, 25, 22);
        uint32_t src1 = getbits(instr, 21, 18);
        uint32_t src2 = getbits(instr, 17, 14);

        if (entry->num_operands == 0) {
            snprintf(out, outlen, "%s", entry->name);
        } else if (entry->num_operands == 1) {
            snprintf(out, outlen, "%s R%u", entry->name, dest);
        } else if (entry->num_operands == 2) {
            snprintf(out, outlen, "%s R%u, R%u", entry->name, dest, src1);
        } else {
            snprintf(out, outlen, "%s R%u, R%u, R%u", entry->name, dest, src1, src2);
        }
        break;
    }

    case I: {
        /* I-type: rd (bits 25..22), rn (bits 21..18), imm16 (bits 17..2) */
        uint32_t rd = getbits(instr, 25, 22);
        uint32_t rn = getbits(instr, 21, 18);
        uint32_t imm_field = getbits(instr, 17, 2);
        int32_t imm_signed = sign_extend((int32_t)imm_field, 16);

        /* format immediate as hex + signed decimal for readability */
        char immbuf[32];
        if (imm_signed < 0) snprintf(immbuf, sizeof immbuf, "-0x%X (%d)", (uint32_t)(-imm_signed), imm_signed);
        else               snprintf(immbuf, sizeof immbuf, "0x%X (%d)", (uint32_t)imm_signed, imm_signed);

        if (entry->num_operands == 0) {
            snprintf(out, outlen, "%s", entry->name);
        } else if (entry->num_operands == 1) {
            snprintf(out, outlen, "%s R%u", entry->name, rd);
        } else if (entry->num_operands == 2) {
            /* unlikely for pure I-type, but handle defensively */
            snprintf(out, outlen, "%s R%u, R%u", entry->name, rd, rn);
        } else { /* 3 operands: rd, rn, imm16 */
            snprintf(out, outlen, "%s R%u, R%u, #%s", entry->name, rd, rn, immbuf);
        }
        break;
    }

    case RI: {
        /* RI-type: supports both register and immediate forms via LSB marker (instr & 1) */
        uint32_t dest = getbits(instr, 25, 22);
        bool imm_used = (instr & 1u) != 0;

        if (!imm_used) {
            uint32_t src1 = getbits(instr, 21, 18);
            if (entry->num_operands <= 1) {
                snprintf(out, outlen, "%s R%u", entry->name, dest);
            } else if (entry->num_operands == 2) {
                snprintf(out, outlen, "%s R%u, R%u", entry->name, dest, src1);
            } else {
                uint32_t src2 = getbits(instr, 17, 14);
                snprintf(out, outlen, "%s R%u, R%u, R%u", entry->name, dest, src1, src2);
            }
        } else {
            uint32_t imm_field = getbits(instr, 17, 2);
            int32_t imm_signed = sign_extend((int32_t)imm_field, 16);
            char immbuf[32];
            if (imm_signed < 0) snprintf(immbuf, sizeof immbuf, "-0x%X (%d)", (uint32_t)(-imm_signed), imm_signed);
            else               snprintf(immbuf, sizeof immbuf, "0x%X (%d)", (uint32_t)imm_signed, imm_signed);

            if (entry->num_operands <= 1) {
                snprintf(out, outlen, "%s R%u", entry->name, dest);
            } else if (entry->num_operands == 2) {
                snprintf(out, outlen, "%s R%u, #%s", entry->name, dest, immbuf);
            } else {
                uint32_t src1 = getbits(instr, 21, 18);
                snprintf(out, outlen, "%s R%u, R%u, #%s", entry->name, dest, src1, immbuf);
            }
        }
        break;
    }

    case M: {
        const unsigned hi = (32 - 6);
        const unsigned lo  = (32 - 6 - 24);

        uint32_t field = getbits(instr, hi, lo);
        int32_t soff = sign_extend((int32_t)field, 24);
        uint32_t target = (uint32_t)((int32_t)pc + soff);

        snprintf(out, outlen, "%s 0x%08X  (%d)", entry->name, (uint32_t)soff, target);
        break;
    }

    default:
        snprintf(out, outlen, "%s", entry->name);
        break;
    }
}

/* Print a single flag with color if set */
static void print_flag(const char *name, bool set) {
    if (set) printf(ANSI_BOLD ANSI_GREEN "%s " ANSI_RESET, name);
    else      printf(ANSI_DIM ANSI_RED "%s " ANSI_RESET, name);
}

/* Print CPU state with optional previous state to highlight changes */
void print_cpu_state(const Machine* machine, const Machine* prev) {
    /* Header: PC, SP, current instruction word at PC and disasm */
    uint32_t pc = machine->cpu.pc;
    uint32_t sp = machine->cpu.sp;
    uint32_t instr = (machine->mode == BIOS ? machine->rom[pc] : machine->ram[pc]);

    char dis[128];
    disasm(instr, dis, sizeof(dis), pc);

    printf(ANSI_CLEAR_SCREEN);
    printf(ANSI_BOLD "\nCPU STATE\n" ANSI_RESET);
    printf(ANSI_CYAN "PC: " ANSI_RESET "0x%08X    " ANSI_CYAN "SP: " ANSI_RESET "0x%08X    " ANSI_CYAN "MODE: " ANSI_RESET "%s    " ANSI_CYAN "CYCLE: " ANSI_RESET "%d\n", pc, sp, (machine->mode == BIOS ? "BIOS" : "KERNEL"), machine->cpu.cycle);
    printf(ANSI_YELLOW "INST@PC: " ANSI_RESET "0x%08X    " ANSI_CYAN "%-40s\n\n", instr, dis);

    /* Registers printed in two rows of 8 for compactness */
    printf(ANSI_BOLD "Registers\n" ANSI_RESET);
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 8; col++) {
            int i = row * 8 + col;
            uint32_t val = machine->cpu.registers[i];
            bool changed = false;
            if (prev) changed = (prev->cpu.registers[i] != val);

            /* highlight changed registers */
            if (changed) {
                printf(ANSI_BG_GREY ANSI_BOLD "R%-2d: 0x%08X" ANSI_RESET, i, val);
            } else {
                printf("R%-2d: 0x%08X", i, val);
            }

            if (col < 7) printf("   ");
        }
        printf("\n");
    }

    /* Flags */
    bool z = getbit(machine->cpu.flags, 0);
    bool c = getbit(machine->cpu.flags, 1);
    bool n = getbit(machine->cpu.flags, 2);
    bool o = getbit(machine->cpu.flags, 3);

    printf("\n" ANSI_BOLD "Flags: " ANSI_RESET);
    print_flag("Z", z);
    print_flag("C", c);
    print_flag("N", n);
    print_flag("O", o);
    printf("\n");

    /* Footer with small legend */
    printf("\n" ANSI_DIM "Changed registers are highlighted.\n" ANSI_RESET);
    fflush(stdout);
}
