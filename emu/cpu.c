#include "cpu.h"

void cpu_reset(Cpu *cpu) {
    cpu->pc = 0x1000;
    cpu->flags = FLAG_PRV;
    cpu->r = cpu->ksr;
    cpu->running = true;
}
