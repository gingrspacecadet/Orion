#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "machine.h"
#include "ops.h"

void (*ops[])(Machine* machine, uint32_t op) = {
    [0b00000000] = NOP,
    [0b00000100] = MOV,
    [0b00001000] = ADD,
    // [0b00001100] = SUB,
    // [0b00010000] = AND,
    // [0b00010100] = OR,
    // [0b00011000] = XOR,
    // [0b00011100] = LSL,
    // [0b00100000] = LSR,
    // [0b00100100] = ASR,
    // [0b00101000] = LDR,
    // [0b00101100] = STR,
    // [0b00110000] = CMP,
    // [0b00110100] = B,
    // [0b00111000] = BL,
    // [0b00111100] = BEQ,
    // [0b01000000] = BNE,
    // [0b01000100] = BCS,
    // [0b01001000] = BCC,
    // [0b01001100] = BGT,
    // [0b01010000] = BLT,
    // [0b01010100] = PUSH,
    // [0b01011000] = POP,
    [0b01011100] = HALT,
    // [0b01100000] = ADR,
    // [0b01100100] = CMPU,
    // [0b01101000] = MUL,
    // [0b01101100] = DIV,
    // [0b01110000] = LDRB,
    // [0b01110100] = STRB,
    // [0b01111000] = TEST,
    // [0b01111100] = SVC
};

void step(Machine* machine) {
    uint32_t op = fetch(machine);
    uint8_t opcode = getbyte(op, 32);
    if (ops[opcode]) return ops[opcode](machine, op);
    else { printf("Illegal opcode 0x%04X\n", opcode); exit(1); }
}

/* ANSI color helpers for nicer debug output */
#define ANSI_RESET   "\x1b[0m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_DIM     "\x1b[2m"
#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_BG_GREY "\x1b[100m"
#define ANSI_CLEAR_SCREEN "\x1b[2J\x1b[H"

/* Print a single flag with color if set */
static void print_flag(const char *name, bool set) {
    if (set) printf(ANSI_BOLD ANSI_GREEN "%s " ANSI_RESET, name);
    else      printf(ANSI_DIM ANSI_RED "%s " ANSI_RESET, name);
}

/* Print CPU state with optional previous state to highlight changes */
void print_cpu_state(const Machine* machine, const Machine* prev) {
    /* Header: PC, SP, current instruction word at PC */
    uint32_t pc = machine->cpu.pc;
    uint32_t sp = machine->cpu.sp;
    uint32_t instr = machine->memory[pc];

    printf(ANSI_CLEAR_SCREEN);
    printf(ANSI_BOLD "\nCPU STATE\n" ANSI_RESET);
    printf(ANSI_CYAN "PC: " ANSI_RESET "0x%08X    " ANSI_CYAN "SP: " ANSI_RESET "0x%08X\n", pc, sp);
    printf(ANSI_YELLOW "INST@PC: " ANSI_RESET "0x%08X\n\n", instr);

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

int main(int argc, char** argv) {
    #ifdef DEBUG
    puts("\n\n\n");
    #endif

    if (argc < 2) {
        printf("Missing input file");
        return 1;
    }

    FILE* src = fopen(argv[1], "rb");
    if (!src) {
        perror("fopen");
        return 1;
    }

    uint32_t* program = malloc(sizeof(uint32_t) * 1024);
    if (!program) {
        perror("malloc");
        fclose(src);
        return 1;
    }
    size_t read = fread(program, sizeof(uint32_t), 1024, src);
    (void)read;
    fclose(src);

    static Machine machine = {0};
    for (size_t i = 0; program[i] != (uint32_t)NULL; i++) {
        machine.memory[i] = program[i];
    }
    free(program);

    machine.cpu.running = true;
    machine.cpu.pc = 0;
    machine.cpu.sp = 0xFFFF;

    /* Keep a copy of previous machine state only for visual diffs */
    Machine prev = {0};
    memcpy(&prev, &machine, sizeof(Machine));

    while (machine.cpu.running) {
        step(&machine);

        #ifdef DEBUG
        print_cpu_state(&machine, &prev);
        /* update prev to current for next iteration (visual only) */
        memcpy(&prev, &machine, sizeof(Machine));
        /* small sleep could be added here for readability if desired */
        #endif
    }

    return 0;
}
