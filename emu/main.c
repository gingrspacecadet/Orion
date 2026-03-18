#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "machine.h"
#include "debug.h"
#include "ops.h"
#include "ram.h"

#include "devices/vga.h"
#include "devices/keyboard.h"
#include "devices/block.h"

Machine* global_machine;

void (*ops[])(Machine* m, uint32_t op) = {
    [0x00] = NOP,
    [0x01] = MOV,
    [0x02] = ADD,
    // [0x03] = SUB,
    // [0x04] = AND,
    [0x05] = OR,
    // [0x06] = XOR,
    [0x07] = SHL,
    // [0x08] = SHR,
    // [0x09] = ASR,
    [0x0A] = LDR,
    [0x0B] = STR,
    [0x0C] = CMP,
    [0x0D] = JMP,
    [0x0E] = JE,
    [0x0F] = JNE,
    // [0x10] = JC,
    // [0x11] = JNC,
    // [0x12] = JG,
    // [0x13] = JGE,
    [0x14] = JL,
    [0x15] = JLE,
    [0x16] = PUSH,
    [0x17] = POP,
    [0x18] = HLT,
    // [0x19] = ADR,
    // [0x1A] = CMPU,
    [0x1B] = MUL,
    // [0x1C] = DIV,
    // [0x1D] = LDRB,
    // [0x1E] = STRB,
    // [0x1F] = TEST,
    [0x20] = INT,
    [0x21] = CALL,
    [0x22] = RET,
    [0x23] = IRET,
};

#define unlikely(cond)  __glibc_unlikely(cond)
#define likely(cond)    __glibc_likely(cond)
#define CYCLE_TO_TRIGGER (1024 * 1024)

void step(Machine* m) {
    m->cpu.cycle++;
    if (unlikely(F_CHECK(m->cpu, F_INT) && F_CHECK(m->cpu, F_INT_ENABLED))) {
        uint32_t op = 0b01111100000000000000000000000000;
        op |= m->cpu.interrupt << 2;
        F_CLEAR(m->cpu, F_INT);
        INT(m, op);
    } else {
        uint32_t op = fetch(m);
        uint8_t opcode = getbyte(op, 32) >> 2;
        if (ops[opcode]) { ops[opcode](m, op); return; }
        else { m->cpu.pc--; print_cpu_state(m, m);  printf("Illegal opcode 0x%02X\n", opcode); handle_signal(SIGABRT); }
    }
}


int main(int argc, char** argv) {
#ifdef DEBUG
    puts("\n\n\n");
    debug_init();
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
    ram_init();
    m.rom = malloc(sizeof(uint32_t) * ROM_SIZE);
    uint32_t program[1024];
    {
        size_t r = fread(program, sizeof(uint32_t), 1024, src);
        for (size_t i = 0; i < r; i++) {
            bus_write(i, program[i]);
        }
        fclose(src);
    }
    
    block_state = block_init("orion.img");
    block_device.state = block_state;

    bus_register(&block_device);
    bus_register(&vga_device);
    bus_register(&kbd_device);

    m.cpu.running = true;
    m.cpu.pc = 0x0;
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

        if (step_mode) {
            print_cpu_state(&m, &prev);
            printf(ANSI_DIM "Step-by-step mode ENABLED. Press Enter to advance, Space to toggle.\n" ANSI_RESET);
            fflush(stdout);

            for (;;) {
                int c = getchar();
                if (c == '\r' || c == '\n') {
                    break;
                } else if (c == ' ') {
                    step_mode = false;
                    print_cpu_state(&m, &prev);
                    printf(ANSI_DIM "Step-by-step mode DISABLED. Running...\n" ANSI_RESET);
                    fflush(stdout);
                    break;
                } else {
                    kbd_push(&kbd_device, c);
                    m.cpu.interrupt = 1;
                    F_SET(m.cpu, F_INT);
                }
            }

            step(&m);
            memcpy(&prev, &m, sizeof(Machine));
            continue;
        } else {
            step(&m);
            if (unlikely(m.cpu.cycle % CYCLE_TO_TRIGGER == 0)) {
                if (stdin_has_data()) {
                    int c = getchar();
                    if (c == ' ') {
                        step_mode = !step_mode;
                        print_cpu_state(&m, &prev);
                        printf(ANSI_DIM "Step-by-step mode %s. Press Enter to advance, Space to toggle.\n" ANSI_RESET,
                            step_mode ? "ENABLED" : "DISABLED");
                        fflush(stdout);
                        while (stdin_has_data()) (void)getchar();
                    } else {
                        kbd_push(&kbd_device, c);
                        m.cpu.interrupt = 1;
                        F_SET(m.cpu, F_INT);
                    }
                }
                print_cpu_state(&m, &prev);
                memcpy(&prev, &m, sizeof(Machine));
            }
        }

#else
        step(&m);
#endif
    }

    m.cpu.pc--;
    print_cpu_state(&m, &m);
    dump_machine_state(&m);

    free(m.ram);
    free(m.rom);
    return 0;
}
