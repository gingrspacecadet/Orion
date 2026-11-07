#ifndef DEBUG_H
#define DEBUG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "da.h"

typedef struct {
    uint32_t id;
    uint32_t address;
} DebugBreakpoint;

typedef struct {
    da_type(DebugBreakpoint) breakpoints;
    uint32_t next_id;
} Debugger;

typedef struct {
    void *context;

    bool (*is_running)(void *context);
    void (*step)(void *context);
    void (*print_state)(void *context);
    uint32_t (*pc)(void *context);
    bool (*read)(void *context, uint32_t addr, uint32_t *value);

    bool (*resolve_symbol)(void *context, const char *name, uint32_t *address);
    void (*list_symbols)(void *context, const char *filter);
} DebuggerTarget;

void debug_init(Debugger *dbg);
void debug_destroy(Debugger *dbg);

bool debug_add_breakpoint(Debugger *dbg, uint32_t address, uint32_t *out_id);

bool debug_remove_breakpoint(Debugger *dbg, uint32_t id);
bool debug_has_breakpoint(const Debugger *dbg, uint32_t address);

bool debug_shell(Debugger *dbg, const DebuggerTarget *target);

#endif