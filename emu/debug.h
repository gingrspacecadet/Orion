#ifndef DEBUG_H
#define DEBUG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "machine.h"
#include "da.h"

typedef struct {
    uint32_t id;
    uint32_t address;
} DebugBreakpoint;

typedef struct {
    da_type(DebugBreakpoint) breakpoints;
    uint32_t next_id;
} Debugger;

void debug_init(Debugger *dbg);
void debug_destroy(Debugger *dbg);

bool debug_add_breakpoint(Debugger *dbg, uint32_t address, uint32_t *out_id);

bool debug_remove_breakpoint(Debugger *dbg, uint32_t id);
bool debug_has_breakpoint(const Debugger *dbg, uint32_t address);

bool debug_shell(Debugger *dbg, Machine *machine);

#endif