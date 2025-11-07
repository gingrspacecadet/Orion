#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define FLAG_ZERO 0
#define FLAG_CARRY 1
#define FLAG_NEGATIVE 2
#define FLAG_OVERFLOW 3

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

#define fetch(machine) (machine->memory[machine->cpu.pc++])
#define getbit(target, bit) ((target & (1 << bit)) >> bit)
#define OP(name) void name(Machine* machine, uint32_t op)

static inline uint8_t getbyte(uint32_t target, uint16_t start) {
    uint8_t _val = 0;
    for (int i = 0; i < start; i++) {
        _val |= getbit(target, (start - i)) << (8 - i);
    };
    return _val;
}

static inline uint32_t extend(uint16_t target) {
    if (getbit(target, (sizeof((target) * 2) - 1)) == 1) { 
        for (size_t i = 0; i < (31 - (sizeof((target) * 2) - 1)); i++) { 
            target |= 1 << i; 
        }
    }
    return target;
}

OP(NOP) {
    (void)machine; (void)op;
    return;
}

OP(MOV) {
    if (getbit(op, 0) == 0) { // R-type
        uint8_t reg1 = getbyte(op, 32 - 6);
        uint8_t reg2 = reg1 ^ ((reg1 >> 4) << 4);
        reg1 = reg1 >> 4;
        machine->cpu.registers[reg1] = machine->cpu.registers[reg2];
    } else { // I-type
        uint8_t reg = getbyte(op, 32 - 6) >> 4;
        uint32_t imm = (getbyte(op, 32 - (6 + 4 + 4)) << 8) | getbyte(op, 32 - (6 + 4 + 8));
        extend(imm);
        machine->cpu.registers[reg] = imm;
    }
}

OP(HALT) {
    (void)op;
    machine->cpu.running = false;
}

void (*ops[])(Machine* machine, uint32_t op) = {
    [0b00000000] = NOP,
    [0b00000100] = MOV,
    [0b01011100] = HALT,
};

void step(Machine* machine) {
    uint32_t op = fetch(machine);
    uint8_t opcode = getbyte(op, 32);
    if (ops[opcode]) return ops[opcode](machine, op);

    return;
}

#ifdef DEBUG
#include <stdio.h>

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

#endif

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