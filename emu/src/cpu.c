#include <cpu.h>
#include <xmalloc.h>

Cpu *cpu_create() {
    Cpu *cpu = xcalloc(sizeof(Cpu));

    return cpu;
}

void cpu_reset(Cpu *cpu) {
    if (!cpu) return;

    cpu->mode = BIOS;
    cpu->pc = 0;
    cpu->registers[RSP] = -1;
    cpu->running = false;
}