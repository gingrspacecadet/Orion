#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "machine.h"
#include "ops.h"


void (*ops[])(Machine* machine, uint32_t op) = {
    [0b00000000] = NOP,
    [0b00000100] = MOV,
    [0b00001000] = ADD,
    // [0b00001100] = SUB,
    // [0b00010000] = AND,
    // [0b00010100] = OR,
    // [0b00011000] = XOR,
    // [0b00011100] = LSL,
    // [0b00100000] = LSR,
    // [0b00100100] = ASR,
    // [0b00101000] = LDR,
    // [0b00101100] = STR,
    // [0b00110000] = CMP,
    // [0b00110100] = B,
    // [0b00111000] = BL,
    // [0b00111100] = BEQ,
    // [0b01000000] = BNE,
    // [0b01000100] = BCS,
    // [0b01001000] = BCC,
    // [0b01001100] = BGT,
    // [0b01010000] = BLT,
    // [0b01010100] = PUSH,
    // [0b01011000] = POP,
    [0b01011100] = HALT,
    // [0b01100000] = ADR,
    // [0b01100100] = CMPU,
    // [0b01101000] = MUL,
    // [0b01101100] = DIV,
    // [0b01110000] = LDRB,
    // [0b01110100] = STRB,
    // [0b01111000] = TEST,
    // [0b01111100] = SVC
};

void step(Machine* machine) {
    uint32_t op = fetch(machine);
    uint8_t opcode = getbyte(op, 32);
    if (ops[opcode]) return ops[opcode](machine, op);
    else { printf("Illegal opcode 0x%04X\n", opcode); exit(1); }
}

void print_cpu_state(const Machine* machine) {
    printf("PC: %08X SP: %08X\n", machine->cpu.pc, machine->cpu.sp);
    for (int i = 0; i < 16; i++) {
        printf("R%d: %08X\t", i, machine->cpu.registers[i]);
    }
    printf("\nFlags - Z: %d C: %d N: %d O: %d\n",
           getbit(machine->cpu.flags, 0),
           getbit(machine->cpu.flags, 1),
           getbit(machine->cpu.flags, 2),
           getbit(machine->cpu.flags, 3));
}

int main(int argc, char** argv) {
    #ifdef DEBUG
    puts("\n\n\n");
    #endif

    if (argc < 2) {
        printf("Missing input file");
        return 1;
    }

    // uint32_t program[] = { 0b00000100100000001000000000000001, 0b01011100000000000000000000000000, (uint32_t)NULL};
    FILE* src = fopen(argv[1], "rb");
    uint32_t* program = malloc(sizeof(uint32_t) * 1024);
    fread(program, sizeof(uint32_t), 1024, src);
    fclose(src);
    
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