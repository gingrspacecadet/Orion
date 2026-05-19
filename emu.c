#include "gin.h"

typedef struct {
    uint8_t *data;
    size_t size;
    bool readonly;
} Memory;

typedef uint32_t Register;

typedef struct {
    Memory *ram;
    Memory *rom;

    Register registers[16];
    Register pc;
    Register flags;
} Cpu;

typedef enum Flag {
    FLAG_C,
    FLAG_V,
    FLAG_Z,
    FLAG_N,
    FLAG_IE,
    FLAG_RUNNING
} Flag;

void memory_init(Memory *mem, size_t size, bool readonly) {
    mem->readonly = readonly;
    mem->size = size;
    mem->data = xcalloc(size);
}

Cpu *cpu_init(uint32_t pc) {
    Cpu *cpu = xcalloc(sizeof(Cpu));
    cpu->ram = xcalloc(sizeof(Memory));
    cpu->rom = xcalloc(sizeof(Memory));

    memory_init(cpu->ram, 0x1000, false);
    memory_init(cpu->rom, 0x1000, true);

    cpu->pc = pc;
    cpu->registers[15] = cpu->ram->size;
    return cpu;
}

// Instruction encoders for tests
static uint32_t encode_a(uint32_t opcode, uint32_t rn, uint32_t rd, bool regmode, uint32_t rm_or_imm16, bool _signed) {
    // A-type: opcode(6) rn(4) rd(4) (rm(4) | imm(16)) register?(1) _signed?(1)
    uint32_t instr = (opcode & 0x3F) << 26;
    instr |= (rn & 0xF) << 22;
    instr |= (rd & 0xF) << 18;
    if (regmode) {
        instr |= (rm_or_imm16 & 0xF) << 14;
    } else {
        instr |= (rm_or_imm16 & 0xFFFF);
    }
    instr |= (regmode ? 1u : 0u) << 1;
    instr |= (_signed ? 1u : 0u);
    return instr;
}
static uint32_t encode_j(uint32_t opcode, uint32_t cond, bool absolute, uint32_t rm_or_imm16, bool regmode, bool _signed) {
    // J-type: opcode(6) cond(4) absolute?(1) reserved(3) (rm(4) | imm(16)) register?(1) _signed?(1)
    uint32_t instr = (opcode & 0x3F) << 26;
    instr |= (cond & 0xF) << 22;
    instr |= (absolute ? 1u : 0u) << 21;
    // reserved 3 bits left as 0
    if (regmode) instr |= (rm_or_imm16 & 0xF) << 14;
    else instr |= (rm_or_imm16 & 0xFFFF);
    instr |= (regmode ? 1u : 0u) << 1;
    instr |= (_signed ? 1u : 0u);
    return instr;
}
static uint32_t encode_m(uint32_t opcode, uint32_t rn, uint32_t rd, bool regmode, uint32_t rm_or_imm16, bool _signed) {
    // M-type: opcode(6) rn(4) rd(4) (rm(4) | imm(16)) register?(1) _signed?(1)
    uint32_t instr = (opcode & 0x3F) << 26;
    instr |= (rn & 0xF) << 22;
    instr |= (rd & 0xF) << 18;
    if (regmode) instr |= (rm_or_imm16 & 0xF) << 14;
    else instr |= (rm_or_imm16 & 0xFFFF);
    instr |= (regmode ? 1u : 0u) << 1;
    instr |= (_signed ? 1u : 0u);
    return instr;
}

typedef enum Exception {
    EX_INVALID_MEM,
    EX_MISALIGNED_PC,
} Exception;

static uint32_t load32(Memory *mem, uint32_t addr) {
    if (addr + 4 > mem->size) {
        // TODO: raise an exception
        fprintf(stderr, "Invalid memory access 0x%08X\n", addr);
        exit(EX_INVALID_MEM);
    }
    if (addr % 4 != 0) {
        fprintf(stderr, "Misaligned memory access 0x%08X\n", addr);
        exit(EX_INVALID_MEM);
    }
    return (uint32_t)mem->data[addr] | ((uint32_t)mem->data[addr+1] << 8) | ((uint32_t)mem->data[addr+2] << 16) | ((uint32_t)mem->data[addr+3] << 24);
}

static void store32(Memory *mem, uint32_t addr, uint32_t val) {
    if (addr + 4 > mem->size) {
        fprintf(stderr, "Invalid memory access 0x%08X\n", addr);
        exit(EX_INVALID_MEM);
    }
    if (addr % 4 != 0) {
        fprintf(stderr, "Misaligned memory access 0x%08X\n", addr);
        exit(EX_INVALID_MEM);
    }
    mem->data[addr] = val & 0xFF;
    mem->data[addr+1] = (val >> 8) & 0xFF;
    mem->data[addr+2] = (val >> 16) & 0xFF;
    mem->data[addr+3] = (val >> 24) & 0xFF;
}

static uint8_t load8(Memory *mem, uint32_t addr) {
    if (addr >= mem->size) {
        fprintf(stderr, "Invalid memory access 0x%08X\n", addr);
        exit(EX_INVALID_MEM);
    }
    return mem->data[addr];
}

static void store8(Memory *mem, uint32_t addr, uint8_t val) {
    if (addr >= mem->size) {
        fprintf(stderr, "Invalid memory access 0x%08X\n", addr);
        exit(EX_INVALID_MEM);
    }
    mem->data[addr] = val;
}

static inline void set_flag(Cpu *cpu, Flag flag, bool v) {
    if (v) cpu->flags |= (1u << flag);
    else cpu->flags &= ~(1u << flag);
}

static inline bool get_flag(Cpu *cpu, Flag flag) {
    return (cpu->flags >> flag) & 1u;
}

typedef enum Opcode {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_SHL,
    OP_SHR,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_XOR,
    OP_LDR = 0xB,
    OP_STR,
    OP_LDRB,
    OP_STRB,
    OP_JXX,
    OP_CALL,
    OP_RET,
    OP_PUSH,
    OP_POP,
    OP_INTE = 0x20,
    OP_FLAGS,
    OP_HALT,
} Opcode;

static void step(Cpu *cpu) {
    if (!get_flag(cpu, FLAG_RUNNING)) return;
    if (cpu->pc % 4 != 0) {
        fprintf(stderr, "Misaligned PC\n");
        exit(EX_MISALIGNED_PC);
    }
    uint32_t instr = load32(cpu->ram, cpu->pc); cpu->pc += 4;
    uint32_t opcode = (instr >> 26) & 0x3F;

    switch (opcode) {
        case OP_ADD: {
            uint32_t rn = (instr >> 22) & 0xF;
            uint32_t rd = (instr >> 18) & 0xF;
            bool regmode = ((instr >> 1) & 1);
            bool _signed = instr & 1;
            uint32_t operand = regmode ? ((instr >> 14) & 0xF) : (instr & 0xFFFF);
            uint32_t op2 = regmode ? cpu->registers[operand] : operand;
            uint64_t res = (uint64_t)cpu->registers[rn] + (uint64_t)op2;
            cpu->registers[rd] = (uint32_t)res;
            // update flags here
            break;
        }
        
        case OP_SUB: {
            uint32_t rn = (instr >> 22) & 0xF;
            uint32_t rd = (instr >> 18) & 0xF;
            bool regmode = ((instr >> 1) & 1);
            uint32_t operand = regmode ? ((instr >> 14) & 0xF) : (instr & 0xFFFF);
            uint32_t op2 = regmode ? cpu->registers[operand] : operand;
            uint64_t res = (uint64_t)cpu->registers[rn] - (uint64_t)op2;
            cpu->registers[rd] = (uint32_t)res;
            // update flags
            break;
        }
        
        case OP_PUSH: {
            bool regmode = ((instr >> 1) & 1);
            if (regmode) {
                uint32_t rm = (instr >> 14) & 0xF;
                if (cpu->registers[15] < 4) {
                    fprintf(stderr, "STACK OVERFLOW!!\n");
                    exit(1);
                }
                cpu->registers[15] -= 4;
                store32(cpu->ram, cpu->registers[15], cpu->registers[rm]);
            } else {
                uint32_t mask = instr & 0xFFFF;
                for (int r = 0; r < 16; r++) {
                    if (mask & (1u << r)) {
                        if (cpu->registers[15] < 4) {
                            fprintf(stderr, "STACK OVERFLOW!!\n");
                            exit(1);
                        }
                        cpu->registers[15] -= 4;
                        store32(cpu->ram, cpu->registers[15], cpu->registers[r]);
                    }
                }
            }
            break;
        }

        case OP_POP: {
            bool regmode = ((instr >> 1) & 1);
            if (regmode) {
                uint32_t rm = (instr >> 14) & 0xF;
                if (cpu->registers[15] + 4 > cpu->ram->size) {
                    fprintf(stderr, "stack underflow!!\n");
                    exit(1);
                }
                cpu->registers[rm] = load32(cpu->ram, cpu->registers[15]);
                cpu->registers[15] += 4;
            } else {
                uint32_t mask = instr & 0xFFFF;
                for (int r = 15; r >= 0; r--) {
                    if (mask & (1u << r)) {
                        if (cpu->registers[15] + 4 > cpu->ram->size) {
                            fprintf(stderr, "stack underflow!!\n");
                            exit(1);
                        }
                        cpu->registers[r] = load32(cpu->ram, cpu->registers[15]);
                        cpu->registers[15] += 4;
                    }
                }
            }
            break;
        }

        case OP_FLAGS: {
            uint32_t rd = (instr >> 18) & 0xF;
            cpu->registers[rd] = cpu->flags;
            break;
        }

        case OP_HALT: {
            set_flag(cpu, FLAG_RUNNING, false);
            break;
        }

        default: {
            fprintf(stderr, "Invalid opcode %d\n", opcode);
            exit(1);
        }
    }
}

static void run(Cpu *cpu) {
    set_flag(cpu, FLAG_RUNNING, true);
    while (get_flag(cpu, FLAG_RUNNING)) {
        step(cpu);
    }
}

int main(int argc, char **argv) {
    Cpu *cpu = cpu_init(0x0);

    uint32_t p = 0;
    store32(cpu->ram, p, encode_a(OP_ADD, 0, 1, false, 5, false)); p += 4;
    store32(cpu->ram, p, encode_a(OP_ADD, 0, 2, false, 7, false)); p += 4;
    store32(cpu->ram, p, encode_a(OP_ADD, 1, 3, true, 2, false)); p += 4;
    store32(cpu->ram, p, encode_m(OP_PUSH, 0, 0, true, 3, false)); p += 4;
    store32(cpu->ram, p, encode_m(OP_POP, 0, 0, true, 4, false)); p += 4;
    store32(cpu->ram, p, encode_a(OP_FLAGS, 0, 5, false, 0, false)); p += 4;
    store32(cpu->ram, p, encode_a(OP_HALT, 0, 0, 0, 0, 0));

    for (int i = 0; i < 16; i++) printf("R%02d = 0x%08X\n", i, cpu->registers[i]);
    
    run(cpu);
}