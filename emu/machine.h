#ifndef MACHINE_H
#define MACHINE_H

#include "cpu.h"
#include "icu.h"
#include "mmio.h"
#include "mtu.h"
#include "da.h"

typedef struct {
    char *name;
    uint32_t address;
} DebugSymbol;

typedef struct {
    Cpu cpu;
    Icu icu;
    Mmio mmio;
    Mtu mtu;

    da_type(DebugSymbol) debug_symbols;
} Machine;

bool machine_step(Machine *machine);

MtuResult machine_translate_noexcept(Machine *machine, uint32_t virtual_address, MtuAccess access, uint32_t *physical);
MtuResult machine_read32_noexcept(Machine *machine, uint32_t addr, uint32_t *value);
bool machine_read32(Machine *machine, uint32_t addr, uint32_t *value);
MtuResult machine_write32_noexcept(Machine *machine, uint32_t addr, uint32_t value);
bool machine_write32(Machine *machine, uint32_t addr, uint32_t value);
bool machine_read8(Machine *machine, uint32_t addr, uint8_t *out);
bool machine_write8(Machine *machine, uint32_t addr, uint8_t value);

typedef struct {
    bool success;
    uint32_t value;
} MachineFetchResult;

MachineFetchResult machine_fetch32(Machine *machine, uint32_t address);
bool machine_raise_exception(Machine *machine, uint8_t vector, uint32_t return_pc);
#endif