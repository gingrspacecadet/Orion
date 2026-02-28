#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>

#define REGISTERS   32

typedef uint32_t Register;

typedef enum {
    RSP,
} RegisterType;

typedef enum {
    BIOS,
    KERNEL,
    USER,
} CpuMode;

typedef struct {
    Register registers[REGISTERS];
    Register pc;
    CpuMode mode;
    bool running;
} Cpu;

Cpu *cpu_create();
void cpu_reset(Cpu *cpu);

#endif