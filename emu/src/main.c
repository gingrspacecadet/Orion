#include <stdint.h>
#include <stdbool.h>
#include <xmalloc.h>
#include <error.h>

#include <ram.h>
#include <cpu.h>
#include <machine.h>
#include <rom.h>

#include <handlers.h>

int main() {
    Machine *machine = machine_create();

    Ram *ram = ram_create(1024);
    machine_bind_ram(machine, ram);

    Rom *rom = rom_create(1024);
    machine_bind_rom(machine, rom);

    Cpu *cpu = cpu_create();
    cpu_reset(cpu);
    machine_bind_cpu(machine, cpu);

    machine_start(machine);

    machine->cpu->mode = KERNEL;//tmp
    while (true) {
        machine_step(machine);
    }
}