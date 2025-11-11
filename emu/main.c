#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "machine.h"
#include "ops.h"
#include "debug.h"

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
    [0b00110100] = B,
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

    #ifdef DEBUG
    /* step_mode: when true, do not auto-step; only step on Enter.
       Space toggles step_mode at any time. Start enabled. */
    bool step_mode = true;
    tty_enable_raw();
    atexit(tty_restore);
    #endif

    while (machine.cpu.running) {
        #ifdef DEBUG
        /* Poll stdin: if space pressed toggle step_mode immediately.
           If in step_mode, wait for Enter to proceed; otherwise proceed immediately. */

        /* Non-blocking poll for space toggle before stepping */
        if (stdin_has_data()) {
            int c = getchar();
            if (c == ' ') {
                step_mode = !step_mode;
                /* show current mode change in footer by printing once */
                print_cpu_state(&machine, &prev);
                printf(ANSI_DIM "Step-by-step mode %s. Press Enter to advance, Space to toggle.\n" ANSI_RESET,
                       step_mode ? "ENABLED" : "DISABLED");
                fflush(stdout);
                /* consume any extra pending input */
                while (stdin_has_data()) (void)getchar();
                /* If we turned off step_mode, continue to step below immediately */
            } else {
                /* if other keys were pressed, ignore them (they may be leftover) */
            }
        }

        if (step_mode) {
            /* in step mode: print state and wait for Enter to step.
               Space while waiting toggles mode; Enter proceeds one step. */
            print_cpu_state(&machine, &prev);
            printf(ANSI_DIM "Step-by-step mode ENABLED. Press Enter to advance, Space to toggle.\n" ANSI_RESET);
            fflush(stdout);

            for (;;) {
                int c = getchar(); /* blocking read; terminal is raw so this returns one char */
                if (c == '\r' || c == '\n') {
                    break; /* advance one step */
                } else if (c == ' ') {
                    step_mode = false;
                    /* show that mode was toggled off and continue without waiting further */
                    print_cpu_state(&machine, &prev);
                    printf(ANSI_DIM "Step-by-step mode DISABLED. Running...\n" ANSI_RESET);
                    fflush(stdout);
                    break;
                } else {
                    /* ignore other keys */ ;
                }
            }

            /* finally perform a single step */
            step(&machine);
            /* update prev to current for next iteration (visual only) */
            memcpy(&prev, &machine, sizeof(Machine));
            continue; /* loop back to handle potential immediate toggles */
        } else {
            /* not in step mode: normal execution, but still show state after each step */
            step(&machine);
            print_cpu_state(&machine, &prev);
            memcpy(&prev, &machine, sizeof(Machine));
            /* small sleep could be added here for readability if desired */
        }

        #else
        step(&machine);
        #endif
    }

    return 0;
}
