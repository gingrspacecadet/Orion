#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t pc;
    uint32_t sp;
    uint32_t registers[16];
    uint8_t flags;
    bool running;
} CPU;

typedef struct {
    CPU cpu;
    uint32_t memory[0xFFFF];
} Machine;

#ifdef DEBUG
#include <stdio.h>

void print_cpu_state(const Machine* machine) {
    printf("PC: %08X SP: %08X\n", machine->cpu.pc, machine->cpu.sp);
    for (int i = 0; i < 16; i++) {
        printf("R%d: %08X ", i, machine->cpu.registers[i]);
    }
    printf("\nFlags - Z: %d C: %d N: %d O: %d\n",
           machine->cpu.flags.zero,
           machine->cpu.flags.carry,
           machine->cpu.flags.negative,
           machine->cpu.flags.overflow);
}

#endif

int main(void) {
    #ifdef DEBUG
    puts("\n\n\n");
    #endif

    uint32_t program[] = {0};
    
    // Initialize the machine
    static Machine machine = {0};
    for (size_t i = 0; program[i] != (uint32_t)NULL; i++) {
        machine.memory[i] = program[i];
    }
    machine.cpu.running = true;
    machine.cpu.pc = 0;
    machine.cpu.sp = 0xFFFF;

    // Start execution
    while (machine.cpu.running) {
        step(&machine);
        #ifdef DEBUG
        puts("\e[5A\r");
        print_cpu_state(&machine);
        #endif
    }

    return 0;
}