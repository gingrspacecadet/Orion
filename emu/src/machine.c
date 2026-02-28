#include <machine.h>
#include <xmalloc.h>

Machine *machine_create() {
    Machine *machine = xmalloc(sizeof(Machine));
    machine->rom = NULL;
    machine->ram = NULL;
    machine->cpu = NULL;

    return machine;
}

void machine_bind_ram(Machine *machine, Ram *ram) {
    if (!machine || !ram) return;

    machine->ram = ram;
}

void machine_bind_rom(Machine *machine, Rom *rom) {
    if (!machine || !rom) return;

    machine->rom = rom;
}

void machine_start(Machine *machine) {
    if (!machine || !machine->cpu) return;

    machine->cpu->running = true;
}

void machine_bind_cpu(Machine *machine, Cpu* cpu) {
    if (!machine || !cpu) return;

    machine->cpu = cpu;
}

void machine_step(Machine *machine) {
    if (!machine || !machine->cpu) return;

    if (!machine->cpu->running) return;

    uint32_t toexec;

    if (machine->cpu->mode == BIOS) {
        // TODO
    } else {
        // Very basic example
        toexec = ram_fetch32(machine->ram, machine->cpu->pc);

        uint8_t opcode = toexec >> 24 >> 2;

        jumptable[opcode](machine);

        machine->cpu->pc++;
    }
}