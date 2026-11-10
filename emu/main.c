#define X_OPCODES_IMPL
#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "debug.h"
#include "mmio.h"
#include "isa.h"
#include "obj.h"
#include "icu.h"
#include "pit.h"
#include "pcu.h"
#include "mtu.h"
#include "machine.h"

#define BATCH_SIZE 1024

static void fclosep(FILE **f) {
    if (f && *f) fclose(*f);
}

bool load_exe(Machine *machine, char *path) {
    FILE *f __attribute__((cleanup(fclosep))) = fopen(path, "rb");
    exe_header_t header;
    fread(&header, sizeof(exe_header_t), 1, f);

    if (header.magic != EXE_MAGIC) return false;
    if (header.version != 0x01010100) return false;

    exe_section_t sections[header.sections];
    fread(sections, sizeof(exe_section_t), header.sections, f);

    exe_symbol_t symbols[header.symbols];
    fread(symbols, sizeof(exe_symbol_t), header.symbols, f);

    char string_pool[header.string_table_size];
    fread(string_pool, sizeof(char), header.string_table_size, f);

    for (uint32_t i = 0; i < header.sections; i++) {
        exe_section_t section = sections[i];

        memset(&machine->mmio.mem[section.address + section.file_size], 0, section.memory_size - section.file_size);
        long pos = ftell(f);
        fseek(f, section.offset, SEEK_SET);
        fread(&machine->mmio.mem[section.address], sizeof(uint8_t), section.file_size, f);
        fseek(f, pos, SEEK_SET);
    }

    if (!machine->debug_symbols.data)
        da_init(&machine->debug_symbols);

    for (uint32_t i = 0; i < header.symbols; i++) {
        exe_symbol_t symbol = symbols[i];

        DebugSymbol dbgsym = {
            .name = strdup(&string_pool[symbol.name]),
            .address = symbol.address,
        };

        da_push(&machine->debug_symbols, dbgsym);
    }

    return true;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s image.bin\n", argv[0]);
        return 1;
    }

    Machine machine = {0};

    mmio_init(&machine.mmio);

    if (!load_exe(&machine, argv[1])) {
        fprintf(stderr, "Failed to open file %s\n", argv[1]);
        return 1;
    }
    
    if (argc == 3) {
        if (!load_exe(&machine, argv[2])) {
            fprintf(stderr, "Failed to open file %s\n", argv[2]);
            return 1;
        }
    }

    cpu_reset(&machine.cpu);
    machine.icu = (Icu){0};
    Pit pit = {0};
    TimerPool timer_pool = {0};
    pit_init(&pit, &timer_pool, icu_get_irq(&machine.icu, 0));
    pit.ctrl |= 1 | 3 | 7;
    Pcu pcu = {.mmio = &machine.mmio};

    mmio_add_device(&machine.mmio, (Device){.base_addr = ICU, .size = 80, .state = &machine.icu, .read = icu_read, .write = icu_write});
    mmio_add_device(&machine.mmio, (Device){.base_addr = PIT, .size = 24, .state = &pit, .read = pit_read, .write = pit_write});
    mmio_add_device(&machine.mmio, (Device){.base_addr = PCU, .size = 256, .state = &pcu, .read = pcu_read, .write = pcu_write});
    mmio_add_device(&machine.mmio, (Device){.base_addr = MTU, .size = 128, .state = &machine.mtu, .read = mtu_read, .write = mtu_write});

    Debugger debug;
    debug_init(&debug);

    uint64_t last_ns;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    last_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    if (!debug_shell(&debug, &machine))
        machine.cpu.running = false;

    while (machine.cpu.running) {
        for (int i = 0; i < BATCH_SIZE && machine.cpu.running; i++) {
            if (debug.breakpoints.len && debug_has_breakpoint(&debug, machine.cpu.pc)) {

                printf("\nBreakpoint hit at 0x%08X\n", machine.cpu.pc);

                if (!debug_shell(&debug, &machine)) {
                    machine.cpu.running = false;
                    break;
                }
            }

            if (!machine.cpu.running)
                break;

            machine_step(&machine);
        }

        if (machine.icu.has_int && (machine.cpu.flags & FLAG_IE)) {
            uint8_t vector;
            
            if (icu_accept_irq(&machine.icu, &vector) != -1) {
                machine_raise_exception(&machine, vector, machine.cpu.pc);
                continue;
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t now_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
        timer_pool.virtual_time += now_ns - last_ns;
        last_ns = now_ns;

        timer_run_expired(&timer_pool);
    }

    debug_destroy(&debug);

    for (int i = 0; i < 8; i++) {
        printf("R%02d = 0x%08X  ", i, machine.cpu.r[i]);
    }

    putchar('\n');

    for (int i = 8; i < 16; i++) {
        printf("R%02d = 0x%08X  ", i, machine.cpu.r[i]);
    }

    putchar('\n');

    return 0;
}