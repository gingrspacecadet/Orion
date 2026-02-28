#ifndef MACHINE_H
#define MACHINE_H

#include <ram.h>
#include <rom.h>
#include <cpu.h>

typedef struct {
    Rom *rom;
    Ram *ram;
    Cpu *cpu;
} Machine;

Machine *machine_create();
void machine_bind_ram(Machine *machine, Ram *ram);
void machine_bind_rom(Machine *machine, Rom *rom);
void machine_start(Machine *machine);
void machine_bind_cpu(Machine *machine, Cpu* cpu);

void machine_step(Machine *machine);

#endif