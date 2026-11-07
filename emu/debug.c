#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"

#define DISASM_IMPL
#include "disasm.h"

#define INPUT_SIZE 256

void debug_init(Debugger *dbg) {
    memset(dbg, 0, sizeof(*dbg));
    dbg->next_id = 1;
}

void debug_destroy(Debugger *dbg) {
    da_free(&dbg->breakpoints);
    memset(dbg, 0, sizeof(*dbg));
}

bool debug_add_breakpoint(Debugger *dbg, uint32_t address, uint32_t *out_id) {
    for (size_t i = 0; i < dbg->breakpoints.len; i++) {
        if (dbg->breakpoints.data[i].address == address) {
            if (out_id)
                *out_id = dbg->breakpoints.data[i].id;
            return true;
        }
    }

    uint32_t id = dbg->next_id++;

    if (id == 0)
        id = dbg->next_id++;

    DebugBreakpoint breakpoint = (DebugBreakpoint){
        .id = id,
        .address = address,
    };

    da_push(&dbg->breakpoints, breakpoint);

    if (out_id)
        *out_id = id;

    return true;
}

bool debug_remove_breakpoint(Debugger *dbg, uint32_t id) {
    for (size_t i = 0; i < dbg->breakpoints.len; i++) {
        if (dbg->breakpoints.data[i].id != id)
            continue;

        memmove(&dbg->breakpoints.data[i], &dbg->breakpoints.data[i + 1], (dbg->breakpoints.len - i - 1) * sizeof(*dbg->breakpoints.data));

        dbg->breakpoints.len--;
        return true;
    }

    return false;
}

bool debug_has_breakpoint(const Debugger *dbg, uint32_t address) {
    for (size_t i = 0; i < dbg->breakpoints.len; i++) {
        if (dbg->breakpoints.data[i].address == address)
            return true;
    }

    return false;
}

static bool parse_u32(const char *text, uint32_t *out) {
    if (!text || !*text || *text == '-')
        return false;

    errno = 0;

    char *end;
    unsigned long long value = strtoull(text, &end, 0);

    if (errno || end == text || *end || value > UINT32_MAX)
        return false;

    *out = (uint32_t)value;
    return true;
}

static bool debug_resolve_symbol(const Machine *machine, const char *name, uint32_t *address) {
    for (size_t i = 0; i < machine->debug_symbols.len; i++) {
        DebugSymbol sym = machine->debug_symbols.data[i];
        if (strcmp(sym.name, name) == 0) {
            *address = sym.address;
            return true;
        }
    }

    return false;
}

static void debug_list_symbols(Machine *machine, const char *filter) {    
    if (machine->debug_symbols.len == 0) {
        printf("(no symbols)\n");
        return;
    }

    bool found = false;
    for (size_t i = 0; i < machine->debug_symbols.len; i++) {
        DebugSymbol sym = machine->debug_symbols.data[i];
        if (!filter || strstr(sym.name, filter)) {
            printf("0x%08X  \"%s\"\n", sym.address, sym.name);
            found = true;
        }
    }

    if (!found)
        printf("(no matching symbols)\n");
}

static bool parse_location(const Machine *machine, const char *text, uint32_t *out) {
    if (parse_u32(text, out))
        return true;

    return debug_resolve_symbol(machine, text, out);
}

static void debug_print_breakpoints(const Debugger *dbg) {
    for (size_t i = 0; i < dbg->breakpoints.len; i++) {
        const DebugBreakpoint *bp = &dbg->breakpoints.data[i];

        printf("%u: 0x%08" PRIX32 "\n", bp->id, bp->address);
    }
}

static void debug_help(void) {
    puts("help                         Show this help");
    puts("break <address|symbol>       Add a breakpoint");
    puts("breakpoints                  List breakpoints");
    puts("delete <id>                  Remove a breakpoint");
    puts("continue | c                 Resume execution");
    puts("step | s                     Execute one instruction");
    puts("regs                         Show registers and flags");
    puts("symbols [filter]             List executable symbols");
    puts("quit | q                     Terminate the emulator");
}

static void debug_print_state(Machine *machine) {
    printf("PC    = 0x%08X\n", machine->cpu.pc);
    printf("FLAGS = 0x%08X\n", machine->cpu.flags);

    for (int i = 0; i < 16; i++) {
        printf("R%02d   = 0x%08X\n", i, machine->cpu.r[i]);
    }
}

static bool debug_read(Machine *machine, uint32_t addr, uint32_t *value) {
    MtuResult res = machine_read32_noexcept(machine, addr, value);
    if (res != MTU_SUCCESS)
        return false;

    return true;
}

static void print_tlb_entry(Machine *machine, uint32_t index) {
    uint32_t tlb_hi = machine->mtu.tlb_entries[index] >> 32;
    uint32_t tlb_lo = machine->mtu.tlb_entries[index];

    uint32_t vpn = (tlb_hi >> 12) & 0xFFFFF;
    uint32_t asid = (tlb_hi >> 2) & 0x3FF;
    uint32_t pfn = (tlb_lo >> 12) & 0xFFFFF;

    printf("entry %d - Valid = %s, ASID = %d, VPN = 0x%04X, PFN = 0x%04X\n", index, (tlb_hi & 1) ? "true" : "false", asid, vpn, pfn);
}

bool debug_shell(Debugger *dbg, Machine *machine) {
    if (!dbg || !machine)
        return false;

    char line[INPUT_SIZE];

    for (;;) {
        printf("(orion) ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            return false;

        char *cmd = strtok(line, " \t\r\n");
        if (!cmd)
            continue;

        if (!strcmp(cmd, "help")) {
            debug_help();
            continue;
        }

        if (!strcmp(cmd, "break") || !strcmp(cmd, "b")) {
            char *arg = strtok(NULL, " \t\r\n");
            uint32_t address;

            if (!arg || !parse_location(machine, arg, &address)) {
                puts("invalid address or unknown symbol");
                continue;
            }

            uint32_t id;

            if (!debug_add_breakpoint(dbg, address, &id)) {
                puts("failed to allocate breakpoint");
                continue;
            }

            printf("breakpoint %u at 0x%08" PRIX32 "\n", id, address);

            continue;
        }

        if (!strcmp(cmd, "breakpoints")) {
            debug_print_breakpoints(dbg);
            continue;
        }

        if (!strcmp(cmd, "delete") || !strcmp(cmd, "d")) {
            char *arg = strtok(NULL, " \t\r\n");
            uint32_t id;

            if (!parse_u32(arg, &id)) {
                puts("usage: delete <id>");
                continue;
            }

            if (!debug_remove_breakpoint(dbg, id))
                puts("breakpoint not found");

            continue;
        }

        if (!strcmp(cmd, "continue") || !strcmp(cmd, "c")) {
            return true;
        }

        if (!strcmp(cmd, "step") || !strcmp(cmd, "s")) {
            if (!machine->cpu.running) {
                puts("CPU is halted");
                continue;
            }

            machine_step(machine);
            debug_print_state(machine);

            continue;
        }

        if (!strcmp(cmd, "regs")) {
            debug_print_state(machine);
            continue;
        }

        if (!strcmp(cmd, "symbols")) {
            char *filter = strtok(NULL, " \t\r\n");

            debug_list_symbols(machine, filter);

            continue;
        }

        if (!strcmp(cmd, "disasm") || !strcmp(cmd, "mem")) {
            char *arg1 = strtok(NULL, " \t\r\n");
            char *arg2 = strtok(NULL, " \t\r\n");

            uint32_t address = machine->cpu.pc;
            uint32_t count = 8;
            
            if (arg1 && !parse_location(machine, arg1, &address)) {
                puts("invalid address or unknown symbol");
                continue;
            }

            if (arg2 && !parse_u32(arg2, &count)) {
                puts("invalid count");
                continue;
            }

            for (uint32_t i = 0; i < count; i++) {
                uint32_t word;
                if (!debug_read(machine, address + i * sizeof(uint32_t), &word)) {
                    puts("failed to read data");
                    continue;
                }

                if (!strcmp(cmd, "disasm")) {
                    char buffer[INPUT_SIZE];
                    if (!disasm_fmt(word, machine->cpu.pc, buffer, sizeof(buffer))) {
                        puts("failed to disassemble");
                        continue;
                    }

                    printf("%08" PRIX32 " %s\n", address + i * (uint32_t)sizeof(uint32_t), buffer);
                } else {
                    printf("%08" PRIX32 " 0x%08X\n", address + i * (uint32_t)sizeof(uint32_t), word);
                }
            }

            continue;
        }

        if (!strcmp(cmd, "tlb")) {
            char *arg = strtok(NULL, " \t\r\n");

            if (arg) {
                uint32_t entry;
                if (!parse_u32(arg, &entry)) {
                    puts("invalid entry number");
                    continue;
                }
    
                if (entry > 32) {
                    puts("invalid entry number");
                    continue;
                }

                print_tlb_entry(machine, entry);
            } else {
                for (size_t i = 0; i < 32; i++) {
                    print_tlb_entry(machine, i);
                }
            }

            continue;
        }

        if (!strcmp(cmd, "quit") || !strcmp(cmd, "q"))
            return false;

        puts("unknown command; type 'help'");
    }
}