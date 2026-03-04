#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <xmalloc.h>
#include <error.h>

#define PC  0
#define ROM_SIZE    (1 * 1024 * 1024)

typedef struct {
    uint32_t registers[32];
    uint8_t *ram, *rom;
    bool running;
} Cpu;

Cpu *cpu_init(long mem_size) {
    Cpu *c = xmalloc(sizeof(Cpu));
    *c = (Cpu){
        .ram = xmalloc(mem_size),
        .rom = xmalloc(ROM_SIZE),
        .running = true,
    };

    memset(c->registers, 0x0, 32);

    return c;
}

enum {
    MOV,
};

void cpu_step(Cpu *c) {
    // FETCH
    uint32_t opcode = (c->ram[c->registers[PC] + 0] << 24) |
                    (c->ram[c->registers[PC] + 1] << 16) |
                    (c->ram[c->registers[PC] + 2] << 8)  |
                    (c->ram[c->registers[PC] + 3] << 0);

    printf("opcode: %032b\n", opcode);
    uint8_t instr   = (opcode & 0b11111100000000000000000000000000) >> 24;
    uint8_t rd      = (opcode & 0b00000011110000000000000000000000) >> 22;
    uint8_t rn      = (opcode & 0b00000000001111000000000000000000) >> 18;
    uint8_t rm      = (opcode & 0b00000000000000111100000000000000) >> 14;
    uint8_t shift   = (opcode & 0b00000000000000000011110000000000) >> 10;
    uint16_t imm    = (opcode & 0b00000000000000111111111111111100) >> 2;
    uint32_t addr   = (opcode & 0b00000011111111111111111111111100) >> 2;
    bool type       = (opcode & 0b00000000000000000000000000000001) >> 0;

    switch(instr) {
        case MOV: {
            if (type == 0) {
                // R type
                c->registers[rd] = c->registers[rn];
            } else {
                // I type
                printf("setting %d to %d\n", rd, imm);
                c->registers[rd] = imm;
            }
        }
    }

    c->registers[PC] += 4;
}

int main() {
    Cpu *c = cpu_init(64 * 1024 * 1024);

    memcpy(c->ram, (uint8_t[]){
        0b00000000,0b00000011,0b11111111,0b11111101,
    }, 4);

    cpu_step(c);

    printf("Cpu output: %d\n", c->registers[0]);
    return 0;
}