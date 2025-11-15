#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "machine.h"
#include "debug.h"
#include "ops.h"
#include "fb.h"

Machine* global_machine = NULL;

void (*ops[])(Machine* m, uint32_t op) = {
    [0b00000000] = NOP,
    [0b00000001] = MOV,
    [0b00000010] = ADD,
    // [0b00000011] = SUB,
    // [0b00000100] = AND,
    [0b00000101] = OR,
    // [0b00000110] = XOR,
    [0b00000111] = SHL,
    // [0b00001000] = SHR,
    // [0b00001001] = ASR,
    [0b00001010] = LDR,
    [0b00001011] = STR,
    // [0b00001100] = CMP,
    [0b00001101] = JMP,
    // [0b00001110] = CALL,
    // [0b00001111] = JE,
    // [0b00010000] = JNE,
    // [0b00010001] = JC,
    // [0b00010010] = JNC,
    // [0b00010011] = JG,
    // [0b00010100] = JL,
    [0b00010101] = PUSH,
    [0b00010110] = POP,
    [0b00010111] = HLT,
    // [0b00011000] = ADR,
    // [0b00011001] = CMPU,
    // [0b00011010] = MUL,
    // [0b00011011] = DIV,
    // [0b00011100] = LDRB,
    // [0b00011101] = STRB,
    // [0b00011110] = TEST,
    [0b00011111] = INT,
    [0b00100000] = CALL,
    [0b00100001] = RET,
    [0b00100010] = IRET,
};

void step(Machine* m) {
    m->cpu.cycle++;
    if (m->cpu.cycle % 1000 == 0) {
        F_SET(m->cpu, F_INT);
        m->cpu.interrupt = 0;
    }
    if (F_CHECK(m->cpu, F_INT) && F_CHECK(m->cpu, F_INT_ENABLED)) {
        uint32_t op = 0b01111100000000000000000000000000;
        op |= m->cpu.interrupt << 2;
        F_CLEAR(m->cpu, F_INT);
        INT(m, op);
    } else {
        uint32_t op = fetch(m);
        uint8_t opcode = getbyte(op, 32) >> 2;
        if (ops[opcode]) return ops[opcode](m, op);
        else { printf("Illegal opcode 0x%04X\n", opcode); exit(1); }
    }
}

fb_State* fb;

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

    Machine m = {0};
    m.ram = malloc(sizeof(uint32_t) * RAM_SIZE);
    m.rom = malloc(sizeof(uint32_t) * ROM_SIZE);
    uint32_t program[1024];
    {
        size_t r = fread(program, sizeof(uint32_t), 1024, src);
        for (size_t i = 0; i < r; i++) {
            m.ram[i] = program[i];
        }
        fclose(src);
    }

    
    fb = fb_init("Orion", FB_W, FB_H);

    m.cpu.running = true;
    m.cpu.pc = 0;
    m.mode = KERNEL;
    m.cpu.sp = RAM_SIZE;
    m.cpu.cycle = 0;
    F_SET(m.cpu, F_INT_ENABLED);
    const char *bios_path = (argc >= 3) ? argv[2] : NULL;
    if (bios_path) {
        FILE *b = fopen(bios_path, "rb");
        if (!b) {
            perror("fopen bios");
            return 1;
        }
        uint32_t bios_buf[0xFFFF];
        size_t r = fread(bios_buf, sizeof(uint32_t), 0xFFFF, b);
        fclose(b);

        for (size_t i = 0; i < r; ++i) {
            m.rom[i] = bios_buf[i];
        }
        m.mode = BIOS;
    }    

    /* Keep a copy of previous m state only for visual diffs */
    Machine prev = {0};
    memcpy(&prev, &m, sizeof(Machine));

    #ifdef DEBUG
    /* step_mode: when true, do not auto-step; only step on Enter.
       Space toggles step_mode at any time. Start enabled. */
    bool step_mode = true;
    tty_enable_raw();
    atexit(tty_restore);
    global_machine = &m;
    signal(SIGINT, handle_signal);   // Ctrl+C
    signal(SIGTERM, handle_signal);  // kill
    signal(SIGABRT, handle_signal);  // abort
    signal(SIGSEGV, handle_signal);  // segmentation fault
    #endif

    while (m.cpu.running) {
        #ifdef DEBUG
        /* Poll stdin: if space pressed toggle step_mode immediately.
           If in step_mode, wait for Enter to proceed; otherwise proceed immediately. */

        /* Non-blocking poll for space toggle before stepping */
        if (stdin_has_data()) {
            int c = getchar();
            if (c == ' ') {
                step_mode = !step_mode;
                /* show current mode change in footer by printing once */
                print_cpu_state(&m, &prev);
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
            print_cpu_state(&m, &prev);
            printf(ANSI_DIM "Step-by-step mode ENABLED. Press Enter to advance, Space to toggle.\n" ANSI_RESET);
            fflush(stdout);

            for (;;) {
                int c = getchar(); /* blocking read; terminal is raw so this returns one char */
                if (c == '\r' || c == '\n') {
                    break; /* advance one step */
                } else if (c == ' ') {
                    step_mode = false;
                    /* show that mode was toggled off and continue without waiting further */
                    print_cpu_state(&m, &prev);
                    printf(ANSI_DIM "Step-by-step mode DISABLED. Running...\n" ANSI_RESET);
                    fflush(stdout);
                    break;
                } else {
                    /* ignore other keys */ ;
                }
            }

            /* finally perform a single step */
            step(&m);
            /* update prev to current for next iteration (visual only) */
            memcpy(&prev, &m, sizeof(Machine));
            continue; /* loop back to handle potential immediate toggles */
        } else {
            /* not in step mode: normal execution, but still show state after each step */
            step(&m);
            print_cpu_state(&m, &prev);
            memcpy(&prev, &m, sizeof(Machine));
            /* small sleep could be added here for readability if desired */
        }

        #else
        step(&m);
        #endif
    }


    free(m.ram);
    free(m.rom);
    fb_destroy(fb);
    return 0;
}
