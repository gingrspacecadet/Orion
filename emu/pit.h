#ifndef PIT_H
#define PIT_H

#include <stdint.h>
#include "timer.h"
#include "isa.h"
#include "irq.h"

typedef struct {
    uint64_t interval;
    uint64_t current;
    uint8_t ctrl;

    TimerPool *pool;
    Timer timer;
    IrqLine irq;
} Pit;

void pit_init(Pit *pit, TimerPool *pool, IrqLine irq);

#endif