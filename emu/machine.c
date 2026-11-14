#include <stdio.h>
#include "machine.h"
#include "isa.h"

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

static inline bool push32(Machine *machine, uint32_t v) {
    R(RSP) -= 4;
    return machine_write32(machine, R(RSP), v);
}

static inline bool pop32(Machine *machine, uint32_t *v) {
    uint32_t val;
    if (!machine_read32(machine, R(RSP), &val)) return false;
    R(RSP) += 4;
    *v = val;
    return true;
}

#define NEXT_INSTR machine->cpu.pc + 4

bool machine_step(Machine *machine) {
    MachineFetchResult res = machine_fetch32(machine, machine->cpu.pc);
    if (!res.success) return false;
    uint32_t word = res.value;

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
                machine_raise_exception(machine, ARITHMETIC_FAULT, NEXT_INSTR);
                return false;
            }

            if (lhs == INT32_MIN && rhs == -1) {
                machine_raise_exception(machine, ARITHMETIC_FAULT, NEXT_INSTR);
                return false;
            }

            R(A_RD(word)) = (uint32_t)(lhs / rhs);
            break;
        }

        case OP_DIVU: {
            uint32_t lhs = R(A_RN(word));
            uint32_t rhs = A_REGISTER(word) ? R(A_OPERAND(word) & 0xF) : A_OPERAND(word);

            if (rhs == 0) {
                machine_raise_exception(machine, ARITHMETIC_FAULT, NEXT_INSTR);
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
                    machine_raise_exception(machine, INVALID_INSTR, NEXT_INSTR);
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
                machine_raise_exception(machine, MISALIGNED_PC, NEXT_INSTR);
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
                machine_raise_exception(machine, MISALIGNED_PC, NEXT_INSTR);
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
                machine_raise_exception(machine, MISALIGNED_DATA, NEXT_INSTR);
                return false;
            }
        
            if (!machine_read32(machine, address, &R(M_RD(word)))) return false;
            break;
        }

        case OP_STR: {
            uint32_t address = R(M_RN(word)) + (M_REGISTER(word) ? (int32_t)R(M_OPERAND(word) & 0xF) : sign_extend_16(M_OPERAND(word)));
            if (address % 4 != 0) {
                machine_raise_exception(machine, MISALIGNED_DATA, NEXT_INSTR);
                return false;
            }
        
            if (!machine_write32(machine, address, R(M_RD(word)))) return false;
            break;
        }

        case OP_LDRB: {
            uint32_t address = R(M_RN(word)) + (M_REGISTER(word) ? (int32_t)R(M_OPERAND(word) & 0xF) : sign_extend_16(M_OPERAND(word)));

            uint8_t value;
            if (!machine_read8(machine, address, &value)) return false;
            R(M_RD(word)) = value;
            break;
        }

        case OP_STRB: {
            uint32_t address = R(M_RN(word)) + (M_REGISTER(word) ? (int32_t)R(M_OPERAND(word) & 0xF) : sign_extend_16(M_OPERAND(word)));

            if (!machine_write8(machine, address, R(M_RD(word)))) return false;
            break;
        }

        case OP_NOP: {
            printf("NOP!\n");
            exit(2);
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
                    machine_raise_exception(machine, PRIV_VIOLATION, NEXT_INSTR);
                    return false;
                }
                if ((value & FLAG_TM) != 0) {
                    machine_raise_exception(machine, INVALID_INSTR, NEXT_INSTR);
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
                machine_raise_exception(machine, PRIV_VIOLATION, NEXT_INSTR);
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
            machine_raise_exception(machine, INVALID_INSTR, NEXT_INSTR);
            return false;
        }
    }

    if (advance) machine->cpu.pc += 4;

    return true;
}

MtuResult machine_translate_noexcept(Machine *machine, uint32_t virtual_address, MtuAccess access, uint32_t *physical) {
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

MtuResult machine_read32_noexcept(Machine *machine, uint32_t addr, uint32_t *value) {
    assert(machine);
    uint32_t phys;
    MtuResult res = machine_translate_noexcept(machine, addr, DATA_READ, &phys);
    if (res != MTU_SUCCESS) return res;

    *value = mmio_read32(&machine->mmio, phys);
    return MTU_SUCCESS;
}

bool machine_read32(Machine *machine, uint32_t addr, uint32_t *value) {
    assert(machine);
    MtuResult res = machine_read32_noexcept(machine, addr, value);
    if (res == MTU_MISS) {
        machine_raise_exception(machine, DTLB_MISS, machine->cpu.pc + 4);
        return false;
    }
    if (res == MTU_PERM) {
        machine_raise_exception(machine, PAGE_FAULT, machine->cpu.pc + 4);
        return false;
    }

    return true;
}

MtuResult machine_write32_noexcept(Machine *machine, uint32_t addr, uint32_t value) {
    assert(machine);
    uint32_t phys;
    MtuResult res = machine_translate_noexcept(machine, addr, DATA_WRITE, &phys);
    if (res != MTU_SUCCESS) return res;

    mmio_write32(&machine->mmio, phys, value);
    return MTU_SUCCESS;
}

bool machine_write32(Machine *machine, uint32_t addr, uint32_t value) {
    assert(machine);
    MtuResult res = machine_write32_noexcept(machine, addr, value);
    if (res == MTU_MISS) {
        machine_raise_exception(machine, DTLB_MISS, machine->cpu.pc + 4);
        return false;
    }
    if (res == MTU_PERM) {
        machine_raise_exception(machine, PAGE_FAULT, machine->cpu.pc + 4);
        return false;
    }

    return true;
}

bool machine_read8(Machine *machine, uint32_t addr, uint8_t *out) {
    assert(machine); assert(out);
    uint32_t phys;
    MtuResult res = machine_translate_noexcept(machine, addr, DATA_READ, &phys);
    if (res == MTU_MISS) {
        machine_raise_exception(machine, DTLB_MISS, machine->cpu.pc + 4);
        return false;
    }
    if (res == MTU_PERM) {
        machine_raise_exception(machine, PAGE_FAULT, machine->cpu.pc + 4);
        return false;
    }

    *out = mmio_read8(&machine->mmio, phys);
    return true;
}

bool machine_write8(Machine *machine, uint32_t addr, uint8_t value) {
    assert(machine);

    uint32_t phys;
    MtuResult res = machine_translate_noexcept(machine, addr, DATA_WRITE, &phys);
    if (res == MTU_MISS) {
        machine_raise_exception(machine, DTLB_MISS, machine->cpu.pc + 4);
        return false;
    }
    if (res == MTU_PERM) {
        machine_raise_exception(machine, PAGE_FAULT, machine->cpu.pc + 4);
        return false;
    }

    mmio_write8(&machine->mmio, phys, value);
    return true;
}

MachineFetchResult machine_fetch32(Machine *machine, uint32_t address) {
    if ((address % 4) != 0) {
        machine_raise_exception(machine, MISALIGNED_PC, machine->cpu.pc + 4);
        return (MachineFetchResult){.success = false};
    }

    uint32_t translated;
    MtuResult res = machine_translate_noexcept(machine, address, INSTR_FETCH, &translated);
    if (res == MTU_MISS) {
        machine_raise_exception(machine, ITLB_MISS, machine->cpu.pc + 4);
        return (MachineFetchResult){.success = false};
    }
    if (res == MTU_PERM) {
        machine_raise_exception(machine, PAGE_FAULT, machine->cpu.pc + 4);
        return (MachineFetchResult){.success = false};
    }

    return (MachineFetchResult){.success = true, .value = mmio_read32(&machine->mmio, translated)};
}

bool machine_raise_exception(Machine *machine, uint8_t vector, uint32_t return_pc) {
    // fprintf(stderr, "Fault %d raised\n", vector);
    // if (vector < 0x20 || vector == 0xFF) {
    //     fprintf(stderr, "Offending instruction at 0x%08X\n", machine->cpu.pc);        
    // }
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
    if (machine_write32_noexcept(machine, *sp, flags) != MTU_SUCCESS) {
        machine->cpu.running = false;
        fprintf(stderr, "Double Fault\n");
        return false; // double fault!
    }
    *sp -= 4;
    if (machine_write32_noexcept(machine, *sp, return_pc) != MTU_SUCCESS) {
        machine->cpu.running = false;
        fprintf(stderr, "Double Fault\n");
        return false; // double fault!
    }

    machine->cpu.pc = mmio_read32(&machine->mmio, IHVT + (uint32_t)vector * 4);

    return true;
}