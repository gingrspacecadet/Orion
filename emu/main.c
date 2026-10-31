#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "mmio.h"
#include "isa.h"
#include "icu.h"
#include "pit.h"
#include "pcu.h"
#include "mtu.h"

static uint8_t *load_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); exit(1); }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { perror("ftell"); exit(1); }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { perror("malloc"); exit(1); }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { perror("fread"); exit(1); }
    fclose(f);
    *out_len = (size_t)sz;
    return buf;
}

static inline void write_u32le(uint8_t *p, size_t offset, uint32_t value) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    value = __builtin_bswap32(value);
#endif
    memcpy(p + offset, &value, sizeof(value));
}

static inline uint32_t read_u32le(const uint8_t *p, size_t offset) {
    uint32_t x;
    memcpy(&x, p + offset, sizeof(x));

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    x = __builtin_bswap32(x);
#endif

    return x;
}

typedef uint32_t Register;

typedef struct {
    Register usr[16];
    Register ksr[16];
    Register *r;
        #define RSP     15
    Register flags;
        #define FLAG_IE  1u << 0
        #define FLAG_PRV 1u << 1
        #define FLAG_VM  1u << 2
        #define FLAG_TM  1u << 3
    Register pc;
    bool int_pin;
    bool running;
} Cpu;

typedef struct {
    Cpu cpu;
    Icu icu;
    Mmio mmio;
    Mtu mtu;
} Machine;

void cpu_reset(Cpu *cpu) {
    cpu->pc = 0x1000;
    cpu->flags = FLAG_PRV;
    cpu->r = cpu->ksr;
    cpu->running = true;
}

#define R(N) machine->cpu.r[N]

static inline int32_t sign_extend_16(uint32_t value) {
    value &= 0xFFFFu;
    return (value & 0x8000u)
        ? (int32_t)value - 0x10000
        : (int32_t)value;
}

static inline int32_t sign_extend_26(uint32_t value) {
    value &= 0x03FFFFFF;
    return (value & 0x02000000)
        ? (int32_t)value - 0x04000000
        : (int32_t)value;
}

static inline int32_t sign_extend_13(uint32_t value) {
    value &= 0x1FFF;
    return (value & 0x1000)
        ? (int32_t)value - 0x2000
        : (int32_t)value;
}

static inline int64_t signed32(uint32_t value) {
    return (value & 0x80000000u)
        ? (int64_t)value - 0x100000000LL
        : (int64_t)value;
}

static inline uint32_t arithmetic_rhs(Machine *machine, uint32_t word) {
    if (A_REGISTER(word))
        return R(A_OPERAND(word) & 0xF);

    return (uint32_t)sign_extend_16(A_OPERAND(word));
}

#define set_reg_bank(cpu) \
    cpu.r = (cpu.flags & FLAG_PRV) ? cpu.ksr : cpu.usr;

bool cpu_raise_exception(Machine *machine, uint8_t vector, uint32_t return_pc);

MtuResult cpu_translate_noexcept(Machine *machine, uint32_t virtual_address, MtuAccess access, uint32_t *physical) {
    assert(machine); assert(physical);

    if ((machine->cpu.flags & FLAG_TM) || !(machine->cpu.flags & FLAG_VM)) {
        *physical = virtual_address;
        return MTU_SUCCESS;
    }

    uint32_t phys;
    MtuResult res = mtu_translate(&machine->mtu, virtual_address, access, !(machine->cpu.flags & FLAG_PRV), &phys);
    if (res == MTU_MISS) return MTU_MISS;
    if (res == MTU_PERM) return MTU_PERM;

    *physical = phys;
    return MTU_SUCCESS;
}

MtuResult cpu_read32_noexcept(Machine *machine, uint32_t addr, uint32_t *value) {
    assert(machine);
    uint32_t phys;
    MtuResult res = cpu_translate_noexcept(machine, addr, DATA_READ, &phys);
    if (res != MTU_SUCCESS) return res;

    *value = mmio_read32(&machine->mmio, phys);
    return MTU_SUCCESS;
}

bool cpu_read32(Machine *machine, uint32_t addr, uint32_t *value) {
    assert(machine);
    MtuResult res = cpu_read32_noexcept(machine, addr, value);
    if (res == MTU_MISS) {
        cpu_raise_exception(machine, DTLB_MISS, machine->cpu.pc + 4);
        return false;
    }
    if (res == MTU_PERM) {
        cpu_raise_exception(machine, PAGE_FAULT, machine->cpu.pc + 4);
        return false;
    }

    return true;
}

MtuResult cpu_write32_noexcept(Machine *machine, uint32_t addr, uint32_t value) {
    assert(machine);
    uint32_t phys;
    MtuResult res = cpu_translate_noexcept(machine, addr, DATA_WRITE, &phys);
    if (res != MTU_SUCCESS) return res;

    mmio_write32(&machine->mmio, phys, value);
    return MTU_SUCCESS;
}

bool cpu_write32(Machine *machine, uint32_t addr, uint32_t value) {
    assert(machine);
    MtuResult res = cpu_write32_noexcept(machine, addr, value);
    if (res == MTU_MISS) {
        cpu_raise_exception(machine, DTLB_MISS, machine->cpu.pc + 4);
        return false;
    }
    if (res == MTU_PERM) {
        cpu_raise_exception(machine, PAGE_FAULT, machine->cpu.pc + 4);
        return false;
    }

    return true;
}

bool cpu_read8(Machine *machine, uint32_t addr, uint8_t *out) {
    assert(machine); assert(out);
    uint32_t phys;
    MtuResult res = cpu_translate_noexcept(machine, addr, DATA_READ, &phys);
    if (res == MTU_MISS) {
        cpu_raise_exception(machine, DTLB_MISS, machine->cpu.pc + 4);
        return false;
    }
    if (res == MTU_PERM) {
        cpu_raise_exception(machine, PAGE_FAULT, machine->cpu.pc + 4);
        return false;
    }

    *out = mmio_read8(&machine->mmio, phys);
    return true;
}

bool cpu_write8(Machine *machine, uint32_t addr, uint8_t value) {
    assert(machine);

    uint32_t phys;
    MtuResult res = cpu_translate_noexcept(machine, addr, DATA_WRITE, &phys);
    if (res == MTU_MISS) {
        cpu_raise_exception(machine, DTLB_MISS, machine->cpu.pc + 4);
        return false;
    }
    if (res == MTU_PERM) {
        cpu_raise_exception(machine, PAGE_FAULT, machine->cpu.pc + 4);
        return false;
    }

    mmio_write8(&machine->mmio, phys, value);
    return true;
}

bool cpu_fetch32(Machine *machine, uint32_t address, uint32_t *value) {
    if ((address % 4) != 0) {
        cpu_raise_exception(machine, MISALIGNED_PC, machine->cpu.pc + 4);
        return false;
    }

    uint32_t translated;
    MtuResult res = cpu_translate_noexcept(machine, address, INSTR_FETCH, &translated);
    if (res == MTU_MISS) {
        cpu_raise_exception(machine, ITLB_MISS, machine->cpu.pc + 4);
        return false;
    }
    if (res == MTU_PERM) {
        cpu_raise_exception(machine, PAGE_FAULT, machine->cpu.pc + 4);
        return false;
    }

    *value = mmio_read32(&machine->mmio, translated);
    return true;
}

bool cpu_raise_exception(Machine *machine, uint8_t vector, uint32_t return_pc) {
    if (vector < 0x20 || vector == 0xFF) {
        fprintf(stderr, "Fault %d raised\n", vector);
        fprintf(stderr, "Offending instruction at 0x%08X\n", machine->cpu.pc);
    }
    uint32_t flags = machine->cpu.flags;
    machine->cpu.flags &= ~FLAG_IE;
    machine->cpu.flags |= FLAG_PRV;
    set_reg_bank(machine->cpu);

    if (vector == ITLB_MISS || vector == DTLB_MISS)
        machine->cpu.flags |= FLAG_TM;

    Register *sp = &machine->cpu.r[RSP];

    if (*sp < 8) {
        machine->cpu.running = false;
        fprintf(stderr, "Double Fault\n");
        return false; // double fault!
    }

    if ((*sp % 4) != 0) {
        machine->cpu.running = false;
        fprintf(stderr, "Double Fault\n");
        return false; // double fault!
    }

    *sp -= 4;
    if (cpu_write32_noexcept(machine, *sp, flags) != MTU_SUCCESS) {
        machine->cpu.running = false;
        fprintf(stderr, "Double Fault\n");
        return false; // double fault!
    }
    *sp -= 4;
    if (cpu_write32_noexcept(machine, *sp, return_pc) != MTU_SUCCESS) {
        machine->cpu.running = false;
        fprintf(stderr, "Double Fault\n");
        return false; // double fault!
    }

    machine->cpu.pc = mmio_read32(&machine->mmio, IHVT + (uint32_t)vector * 4);

    return true;
}

static inline bool push32(Machine *machine, uint32_t v) {
    R(RSP) -= 4;
    return cpu_write32(machine, R(RSP), v);
}

static inline bool pop32(Machine *machine, uint32_t *v) {
    uint32_t val;
    if (!cpu_read32(machine, R(RSP), &val)) return false;
    R(RSP) += 4;
    *v = val;
    return true;
}

#define NEXT_INSTR machine->cpu.pc + 4

bool step(Machine *machine) {
    uint32_t word;
    if (!cpu_fetch32(machine, machine->cpu.pc, &word)) return false;

    bool advance = true;

    switch (OPCODE(word)) {
        case OP_ADD: {
            R(A_RD(word)) = R(A_RN(word)) + arithmetic_rhs(machine, word);
            break;
        }

        case OP_SUB: {
            R(A_RD(word)) = R(A_RN(word)) - arithmetic_rhs(machine, word);
            break;
        }

        case OP_MUL: {
            R(A_RD(word)) = R(A_RN(word)) * arithmetic_rhs(machine, word);
            break;
        }

        case OP_DIV: {
            int64_t lhs = signed32(R(A_RN(word)));
            int64_t rhs = A_REGISTER(word) ? signed32(R(A_OPERAND(word) & 0xF)) : sign_extend_16(A_OPERAND(word));

            if (rhs == 0) {
                cpu_raise_exception(machine, ARITHMETIC_FAULT, NEXT_INSTR);
                return false;
            }

            if (lhs == INT32_MIN && rhs == -1) {
                cpu_raise_exception(machine, ARITHMETIC_FAULT, NEXT_INSTR);
                return false;
            }

            R(A_RD(word)) = (uint32_t)(lhs / rhs);
            break;
        }

        case OP_DIVU: {
            uint32_t lhs = R(A_RN(word));
            uint32_t rhs = A_REGISTER(word) ? R(A_OPERAND(word) & 0xF) : A_OPERAND(word);

            if (rhs == 0) {
                cpu_raise_exception(machine, ARITHMETIC_FAULT, NEXT_INSTR);
                return false;
            }

            R(A_RD(word)) = lhs / rhs;
            break;
        }

        case OP_AND: {
            R(A_RD(word)) = R(A_RN(word)) & (A_REGISTER(word) ? (R(A_OPERAND(word) & 0xF)) : A_OPERAND(word));
            break;
        }

        case OP_OR: {
            R(A_RD(word)) = R(A_RN(word)) | (A_REGISTER(word) ? (R(A_OPERAND(word) & 0xF)) : A_OPERAND(word));
            break;
        }

        case OP_XOR: {
            R(A_RD(word)) = R(A_RN(word)) ^ (A_REGISTER(word) ? (R(A_OPERAND(word) & 0xF)) : A_OPERAND(word));
            break;
        }

        case OP_SHL: {
            R(A_RD(word)) = R(A_RN(word)) << ((A_REGISTER(word) ? (R(A_OPERAND(word) & 0xF)) : A_OPERAND(word)) & 0x1F);
            break;
        }

        case OP_SHR: {
            R(A_RD(word)) = R(A_RN(word)) >> ((A_REGISTER(word) ? (R(A_OPERAND(word) & 0xF)) : A_OPERAND(word)) & 0x1F);
            break;
        }

        case OP_ASHR: {
            R(A_RD(word)) = signed32(R(A_RN(word))) >> ((A_REGISTER(word) ? (R(A_OPERAND(word) & 0xF)) : A_OPERAND(word)) & 0x1F);
            break;
        }

        case OP_LUI: {
            R(A_RD(word)) = (uint32_t)(A_REGISTER(word) ? (R(A_OPERAND(word) & 0xF)) : A_OPERAND(word)) << 16;
            break;
        }

        case OP_JMP: {
            uint32_t target = machine->cpu.pc + 4 + sign_extend_26(J_DISPLACEMENT(word)) * 4;
            machine->cpu.pc = target;
            advance = false;
            break;
        }

        case OP_JXX: {
            uint32_t target = machine->cpu.pc + 4 + sign_extend_13(B_DISPLACEMENT(word)) * 4;

            uint32_t operand = B_REGISTER(word) ? (R(B_OPERAND(word) & 0xF)) : B_OPERAND(word);

            bool jump = false;
            switch (B_COND(word)) {
                case COND_JEQ:  if (R(B_RN(word)) == operand) jump = true; break;
                case COND_JGE:  if (signed32(R(B_RN(word))) >= signed32(operand)) jump = true; break;
                case COND_JGEU: if (R(B_RN(word)) >= operand) jump = true; break;
                case COND_JLT:  if (signed32(R(B_RN(word)))  < signed32(operand)) jump = true; break;
                case COND_JLTU: if (R(B_RN(word)) < operand) jump = true; break;
                case COND_JNE:  if (R(B_RN(word)) != operand) jump = true; break;
                default: {
                    fprintf(stderr, "Unknown jump condition\n");
                    cpu_raise_exception(machine, INVALID_INSTR, NEXT_INSTR);
                    return false;
                }
            }

            if (jump) {
                machine->cpu.pc = target;
                advance = false;
            }

            break;
        }

        case OP_JMPR: {
            if (R(I_RN(word)) % 4 != 0) {
                cpu_raise_exception(machine, MISALIGNED_PC, NEXT_INSTR);
                return false;
            }

            machine->cpu.pc = R(I_RN(word));
            advance = false;
            break;
        }

        case OP_CALL: {
            int32_t target = machine->cpu.pc + 4 + sign_extend_26(J_DISPLACEMENT(word)) * 4;
            push32(machine, machine->cpu.pc + 4);
            machine->cpu.pc = target;
            advance = false;
            break;
        }

        case OP_CALLR: {
            if (R(I_RN(word)) % 4 != 0) {
                cpu_raise_exception(machine, MISALIGNED_PC, NEXT_INSTR);
                return false;
            }

            push32(machine, machine->cpu.pc + 4);

            machine->cpu.pc = R(I_RN(word));
            advance = false;
            break;
        }

        case OP_RET: {
            if (!pop32(machine, &machine->cpu.pc)) return false;
            advance = false;
            break;
        }

        case OP_LDR: {
            uint32_t address = R(M_RN(word)) + (M_REGISTER(word) ? (int32_t)R(M_OPERAND(word) & 0xF) : sign_extend_16(M_OPERAND(word)));
            if (address % 4 != 0) {
                cpu_raise_exception(machine, MISALIGNED_DATA, NEXT_INSTR);
                return false;
            }
        
            if (!cpu_read32(machine, address, &R(M_RD(word)))) return false;
            break;
        }

        case OP_STR: {
            uint32_t address = R(M_RN(word)) + (M_REGISTER(word) ? (int32_t)R(M_OPERAND(word) & 0xF) : sign_extend_16(M_OPERAND(word)));
            if (address % 4 != 0) {
                cpu_raise_exception(machine, MISALIGNED_DATA, NEXT_INSTR);
                return false;
            }
        
            if (!cpu_write32(machine, address, R(M_RD(word)))) return false;
            break;
        }

        case OP_LDRB: {
            uint32_t address = R(M_RN(word)) + (M_REGISTER(word) ? (int32_t)R(M_OPERAND(word) & 0xF) : sign_extend_16(M_OPERAND(word)));

            uint8_t value;
            if (!cpu_read8(machine, address, &value)) return false;
            R(M_RD(word)) = value;
            break;
        }

        case OP_STRB: {
            uint32_t address = R(M_RN(word)) + (M_REGISTER(word) ? (int32_t)R(M_OPERAND(word) & 0xF) : sign_extend_16(M_OPERAND(word)));

            if (!cpu_write8(machine, address, R(M_RD(word)))) return false;
            break;
        }

        case OP_NOP: {
            // printf("NOP!\n");
            break;
        }

        case OP_HALT: {
            machine->cpu.running = false;
            return false;
        }

        case OP_RPC: {
            R(S_REG(word)) = machine->cpu.pc;
            break;
        }

        case OP_FLAGS: {
            if (S_WRITE(word)) {
                uint32_t value = S_REGISTER(word) ? S_REG(word) : S_PAYLOAD(word);
                if ((machine->cpu.flags & FLAG_PRV) == 0) {
                    cpu_raise_exception(machine, PRIV_VIOLATION, NEXT_INSTR);
                    return false;
                }
                if ((value & FLAG_TM) != 0) {
                    cpu_raise_exception(machine, INVALID_INSTR, NEXT_INSTR);
                    return false;
                }
                machine->cpu.flags = value;
                set_reg_bank(machine->cpu);
            } else {
                R(S_REG(word)) = machine->cpu.flags;
            }

            break;
        }

        case OP_IRET: {
            if ((machine->cpu.flags & FLAG_PRV) == 0) {
                cpu_raise_exception(machine, PRIV_VIOLATION, NEXT_INSTR);
                return false;
            }

            if (!pop32(machine, &machine->cpu.pc)) return false;
            if (!pop32(machine, &machine->cpu.flags)) return false;
            set_reg_bank(machine->cpu);

            advance = false;

            break;
        }

        default: {
            fprintf(stderr, "Unknown opcode 0x%X!\n", OPCODE(word));
            exit(1);
            cpu_raise_exception(machine, INVALID_INSTR, NEXT_INSTR);
            return false;
        }
    }

    if (advance) machine->cpu.pc += 4;

    return true;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s image.bin\n", argv[0]);
        return 1;
    }

    size_t imgsz;
    uint8_t *img = load_file(argv[1], &imgsz);

    Machine machine = {0};

    mmio_init(&machine.mmio);

    memcpy(machine.mmio.mem + BOOT, img, imgsz); // temporary!!

    if (argc == 3) {
        img = load_file(argv[2], &imgsz);
        memcpy(machine.mmio.mem + RAM, img, imgsz);
    }

    cpu_reset(&machine.cpu);
    machine.icu = (Icu){0};
    Pit pit = {0};
    TimerPool timer_pool = {0};
    pit_init(&pit, &timer_pool, icu_get_irq(&machine.icu, 0));
    pit.ctrl |= 1 | 3 | 7;
    Pcu pcu = {0};

    mmio_add_device(&machine.mmio, (Device){.base_addr = ICU, .size = 80, .state = &machine.icu, .read = icu_read, .write = icu_write});
    mmio_add_device(&machine.mmio, (Device){.base_addr = PIT, .size = 24, .state = &pit, .read = pit_read, .write = pit_write});
    mmio_add_device(&machine.mmio, (Device){.base_addr = PCU, .size = 256, .state = &pcu, .read = pcu_read, .write = pcu_write});
    mmio_add_device(&machine.mmio, (Device){.base_addr = MTU, .size = 128, .state = &machine.mtu, .read = mtu_read, .write = mtu_write});

    uint64_t last_ns;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    last_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    while (machine.cpu.running) {
        step(&machine);

        if (machine.icu.has_int && (machine.cpu.flags & FLAG_IE)) {
            uint8_t vector;
            
            if (icu_accept_irq(&machine.icu, &vector) != -1) {
                cpu_raise_exception(&machine, vector, machine.cpu.pc);
                continue;
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t now_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
        timer_pool.virtual_time += now_ns - last_ns;
        last_ns = now_ns;

        timer_run_expired(&timer_pool);
    }

    for (int i = 0; i < 8; i++) {
        printf("R%02d = 0x%08X  ", i, machine.cpu.r[i]);
    }

    putchar('\n');

    for (int i = 8; i < 16; i++) {
        printf("R%02d = 0x%08X  ", i, machine.cpu.r[i]);
    }

    putchar('\n');

    return 0;
}