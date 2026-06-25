#include <inttypes.h>
#include <termios.h>
#include <stdbool.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "hw_timer.h"
#include "uart.h"
#include "isa.h"
#include "mem.h"
#include "bus.h"
#include "pcu.h"
#include "icu.h"
#include "gpu.h"

#ifndef DEBUG
    #define INLINE __always_inline
#else
    #define INLINE
#endif

#define unlikely(x) __builtin_expect(!!(x), 0)
#define likely(x) __builtin_expect(!!(x), 1)

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

typedef uint32_t Register;

typedef struct {
    Register r[16];
    
    Register pc;
    Register flags;

    bool running;

    bool int_pin;

    Memory *memory;
    Bus *bus;   // sits on top of `memory`

    uint8_t *pc_page_ptr;
    uint32_t pc_page_num;
} Cpu;

static INLINE void set_flag(Cpu *cpu, Flag flag, bool v) {
    if (v) cpu->flags |= (1u << flag);
    else cpu->flags &= ~(1u << flag);
}

static INLINE bool get_flag(Cpu *cpu, Flag flag) {
    return cpu->flags & (1u << flag);
}

static INLINE void push32_nocheck(Cpu *cpu, const uint32_t v) {
    cpu->r[SP] -= 4;
    bus_write32(cpu->bus, cpu->r[SP], v);
}

static void raise_exception(Cpu *cpu, uint8_t e) {
    fprintf(stderr, "exception"); exit(1);

    // build the exception frame
    // to avoid recursive exceptions
    // dont check sp bounds and hope
    push32_nocheck(cpu, cpu->flags);
    set_flag(cpu, FLAG_IE, false);
    push32_nocheck(cpu, cpu->pc);
    push32_nocheck(cpu, e);
    push32_nocheck(cpu, bus_read32(cpu->bus, cpu->pc));
    uint32_t target = bus_read32(cpu->bus, IHVT_BASE + ((uint32_t)e * 4));
    cpu->pc = target;
}

[[nodiscard]]
static INLINE bool push32(Cpu *cpu, const uint32_t v) {
    if (unlikely(cpu->r[SP] <= 4)) {
        raise_exception(cpu, EX_STACK_OVERFLOW);
        return false;
    }
    cpu->r[SP] -= 4;
    bus_write32(cpu->bus, cpu->r[SP], v);
    return true;
}
[[nodiscard]]
static INLINE bool pop32(Cpu *cpu, uint32_t *out) {
    if (unlikely(cpu->r[SP] > UINT32_MAX - 4)) {
        raise_exception(cpu, EX_STACK_UNDERFLOW);
        return false;
    }
    *out = bus_read32(cpu->bus, cpu->r[SP]);
    cpu->r[SP] += 4;
    return true;
}

static INLINE void sync_pc_cache(Cpu *cpu) {
    uint32_t page_num = cpu->pc >> 12;
    if (unlikely(page_num != cpu->pc_page_num || !cpu->pc_page_ptr)) {
        cpu->pc_page_num = page_num;
        
        if (unlikely(cpu->bus->page_map[page_num] != NULL)) {
            cpu->pc_page_ptr = NULL;
        } else {
            MemPage *p = get_page(cpu->memory, cpu->pc, false);
            cpu->pc_page_ptr = p ? p->data : NULL;
        }
    }
}

#define get_rm_or_imm(d) (d.is_reg ? cpu->r[d.rm] : (d.is_signed ? (int32_t)((int16_t)d.imm) : d.imm))

static INLINE void step(Cpu *cpu) {
    if (unlikely((cpu->pc & 0x3) != 0)) {
        raise_exception(cpu, EX_MISALIGNED_PC);
        return;
    }

    uint32_t word;

    sync_pc_cache(cpu);

    uint32_t offset = cpu->pc & 4095;
    if (likely(cpu->pc_page_ptr && offset <= 4092)) {
        word = *(const uint32_t *)&cpu->pc_page_ptr[offset];
    } else {
        word = bus_read32(cpu->bus, cpu->pc);
    }

    uint8_t opcode   = (word >> 26) & 0x3F;
    bool is_reg      = (word >> IS_REG_SHIFT) & IS_REG_MASK;
    bool is_signed   = (word >> SIGNED_SHIFT) & SIGNED_MASK;
    uint8_t rm       = (word >> IMM_RM_SHIFT) & RM_MASK;
    uint32_t imm     = (word >> IMM_RM_SHIFT) & IMM_MASK;
    
    uint8_t rn       = (word >> RN_SHIFT) & RN_MASK;
    uint8_t rd       = (word >> RD_SHIFT) & RD_MASK;

    uint32_t op_b = is_reg ? cpu->r[rm] : (is_signed ? (int32_t)((int16_t)imm) : imm);
    bool update_pc = true;

    switch (opcode) {
        case OP_ADD: {
            uint32_t a = cpu->r[rn];
            uint32_t res = a + op_b;
            cpu->r[rd] = res;
            break;
        }

        case OP_SUB: {
            uint32_t a = cpu->r[rn];
            uint32_t res = a - op_b;
            cpu->r[rd] = res;
            break;
        }

        case OP_XOR: {
            cpu->r[rd] = cpu->r[rn] ^ op_b;
            break;
        }

        case OP_OR: {
            cpu->r[rd] = cpu->r[rn] | op_b;
            break;
        }

        case OP_SHL: {
            cpu->r[rd] = cpu->r[rn] << op_b;
            break;
        }

        case OP_SHR: {
            cpu->r[rd] = cpu->r[rn] >> op_b;
            break;
        }

        case OP_AND: {
            cpu->r[rd] = cpu->r[rn] & op_b;
            break;
        }

        case OP_LDR: {
            cpu->r[rd] = bus_read32(cpu->bus, cpu->r[rn] + op_b);
            break;
        }

        case OP_LDRB: {
            cpu->r[rd] = bus_read8(cpu->bus, cpu->r[rn] + op_b);
            break;
        }

        case OP_STR: {
            bus_write32(cpu->bus, cpu->r[rn] + op_b, cpu->r[rd]);
            break;
        }

        case OP_STRB: {
            bus_write8(cpu->bus, cpu->r[rn] + op_b, cpu->r[rd]);
            break;
        }

        case OP_LUI: {
            cpu->r[rd] = op_b << 16;
            break;
        }

        case OP_JXX: {
            uint8_t cond = (word >> 22) & 0xF;
            uint8_t rn = (word >> 18) & 0xF;
            uint8_t rd = (word >> 14) & 0xF;
            bool absolute = (word >> 13) & 0x1;
            uint16_t imm = ((word) & 0xFFF) << 2;

            bool jump = false;
            switch (cond) {
                case COND_JEQ: if (cpu->r[rn] == cpu->r[rd]) jump = true; break;
                case COND_JNE: if (cpu->r[rn] != cpu->r[rd]) jump = true; break;
                case COND_JLT: if (cpu->r[rn] < cpu->r[rd]) jump = true; break;
                case COND_JGE: if ((int32_t)cpu->r[rn] >= (int32_t)cpu->r[rd]) jump = true; break;
                case COND_JLTU:if ((int32_t)cpu->r[rn] <  (int32_t)cpu->r[rd]) jump = true; break;
                case COND_JGEU:if (cpu->r[rn] >= cpu->r[rd]) jump = true; break;
                default: raise_exception(cpu, EX_INVALID_INSTR); return;
            }
            break;
        }

        case OP_CALL: {
            if (!push32(cpu, cpu->pc + 4)) return;
            bool is_absolute = (word >> ABS_SHIFT) & ABS_MASK;
            cpu->pc = op_b + (is_absolute ? 0 : cpu->pc);
            update_pc = false;
            break;
        }

        case OP_RET: {
            if (!pop32(cpu, &cpu->pc)) return;
            update_pc = false;
            break;
        }

        case OP_HALT: {
            cpu->running = false;
            break;
        }

        case OP_IRET: {
            if (!pop32(cpu, &cpu->pc)) return;
            if (!pop32(cpu, &cpu->flags)) return;
            update_pc = false;
            break;
        }

        case OP_FLAGS: {
            if (word & 0x1) {   // write
                cpu->flags = (word >> 6) & is_reg ? 0xF : 0xFFFF;
            } else {    // read
                rd = (word >> 6) & 0xF;
                cpu->r[rd] = cpu->flags;
            }
            break;
        }

        default: {
            raise_exception(cpu, EX_INVALID_INSTR);
            return;
        }
    }

    if (update_pc) cpu->pc += 4;
}

static uint8_t find_highest_priority(IcuDevice *icu, uint32_t active) {
    uint8_t highest = 0;
    uint8_t idx = 0xFF;
    for (int i = 0; i < 32; i++) {
        if (active & (1u << i) && icu->prio[i] > highest) {
            highest = icu->prio[i];
            idx = i;
        }
    }
    return idx;
}

static void icu_check_and_fire(Cpu *cpu, IcuDevice *icu) {
    uint32_t active_irqs = icu->irr & ~(icu->imr);
    if (active_irqs == 0 || !get_flag(cpu, FLAG_IE)) return;

    uint8_t irq = find_highest_priority(icu, active_irqs);

    for (int i = 0; i < 32; i++) {
        if ((icu->isr & (1u << i)) && (icu->prio[i] >= icu->prio[irq])) {
            return;
        }
    }

    icu->irr &= ~(1u << irq);
    icu->isr |= (1u << irq);

    icu_update_output_pin(icu);

    if (!push32(cpu, cpu->flags)) return;
    if (!push32(cpu, cpu->pc)) return;
    set_flag(cpu, FLAG_IE, false);

    uint8_t vec = icu->vec[irq] + 0x20;
    cpu->pc = bus_read32(cpu->bus, IHVT_BASE + (vec * 4));
}

volatile sig_atomic_t signalled = false;

void handle_signal(int sig) {
    signalled = true;
}

void setup_signal_handlers(void) {
    struct sigaction sa = {0};
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
}

static struct termios oldt;
static void disable_rawmode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldt);
}
static void enable_rawmode(void) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &oldt);
    t = oldt;
    t.c_cflag &= ~(ICANON | ECHO);
    t.c_lflag &= ~(ECHOCTL);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &t);

    atexit(disable_rawmode);
}

#define BATCH_SIZE  128

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s out.bin\n", argv[0]);
        return 1;
    }

    setup_signal_handlers();
    enable_rawmode();
    atexit(disable_rawmode);

    const char *path = argv[1];

    size_t imglen;
    uint8_t *img = load_file(path, &imglen);

    Cpu cpu = {0};
    cpu.pc = 0x00001000;
    cpu.memory = mem_init();
    cpu.bus = bus_init(cpu.memory);
    cpu.running = true;

    TimerPool timer_pool = {0};

    // initialise the PCU hardware state
    PcuDevice pcu = {0};
    pcu.bus = cpu.bus;

    // the pcu itself
    pcu.slots[0].device_id      = MAKE_DEVICE_ID(0x123, 0x05, 0, 0);
    pcu.slots[0].component_size = PCU_MAX_DEVICES * sizeof(PcuSlot);
    pcu.slots[0].base_addr      = 0x00010600;

    bus_register_device(cpu.bus, 0x00010600, pcu.slots[0].component_size, &pcu, pcu_internal_read, pcu_internal_write);
    
    // a sample UART device
    pcu.slots[1].device_id      = MAKE_DEVICE_ID(0x123, 0x02, 0x1, 0);
    pcu.slots[1].component_size = 0x00000100;
    pcu.slots[1].base_addr      = 0;

    bus_register_device(cpu.bus, 0, pcu.slots[1].component_size, NULL, uart_read, uart_write);
    
    IcuDevice icu = {0};
    icu.cpu_int_pin = &cpu.int_pin;

    bus_register_device(cpu.bus, 0x00010400, 256, &icu, icu_read, icu_write);

    // TODO: register this as a bus device
    HwTimer system_timer;
    hw_timer_init(&system_timer, &timer_pool, icu_get_irq_line(&icu, 0));

    // gpu!
    GpuDevice *gpu = gpu_create(cpu.bus, &timer_pool, icu_get_irq_line(&icu, 1));
    bus_register_device(cpu.bus, 0x00010A00, 16, gpu, gpu_bus_read, gpu_bus_write);

    // TODO: ideally, this would be done by the ROM image
    for (size_t i = 0; i < imglen; i++) {
        mem_write8(cpu.memory, 0x1000 + i, img[i]);
    }

    uint64_t cycles = 0;
    uint64_t last_ns, now_ns, delta_ns;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    last_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    while (true) {
        if (unlikely(signalled)) break;

        if (cpu.running) {
            for (int i = 0; i < BATCH_SIZE; i++) {
                step(&cpu);
                if (unlikely(!cpu.running)) break;
            }
            cycles += BATCH_SIZE;
        }

        clock_gettime(CLOCK_MONOTONIC, &ts);
        now_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
        delta_ns = now_ns - last_ns;
        last_ns = now_ns;

        timer_pool.virtual_time += delta_ns;

        if (unlikely(timer_pool.active_timers && timer_pool.virtual_time >= timer_pool.active_timers->expire_time)) {
            timer_run_expired(&timer_pool);
        }

        if (unlikely(cpu.int_pin && get_flag(&cpu, FLAG_IE))) {
            icu_check_and_fire(&cpu, &icu);
        }
    }
    for (int i = 0; i < 16; i++) {
        printf("R%d: 0x%08X\t", i, cpu.r[i]);
        if (i == 7) putc('\n', stdout);
    }
    printf("\nPC: 0x%08X\tFLAGS: 0x%08X\n", cpu.pc, cpu.flags);

    for (int i = 0; i < cpu.bus->device_count; i++) {
        printf("bus device %d address 0x%08X\n", i, cpu.bus->devices[i].base_addr);
    }

    for (int i = 0; i < 16; i++) {
        if (pcu.slots[i].device_id == 0) continue;

        PcuSlot dev = pcu.slots[i];
        uint16_t vendor = (dev.device_id >> 20) & 0xFFF;
        uint8_t class = (dev.device_id >> 12) & 0xFF;
        uint8_t device = (dev.device_id >> 4) & 0xFF;
        uint8_t revision = dev.device_id & 0xF;

        printf("pcu device %d address 0x%08X, vendor 0x%03X, class 0x%02X, device 0x%02X, revision 0x%01X\n", i, dev.base_addr, vendor, class, device, revision);
    }
    printf("time: %luns\ncycles: %lu\ncycles per second: %llu\n", timer_pool.virtual_time, cycles, cycles * 1000000000ULL / timer_pool.virtual_time);
}