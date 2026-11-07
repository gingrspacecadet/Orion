#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include "mtu.h"

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

void cpu_reset(Cpu *cpu);

#endif