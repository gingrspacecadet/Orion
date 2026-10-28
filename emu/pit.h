#ifndef PIT_H
#define PIT_H

#include <stdint.h>
#include "timer.h"
#include "isa.h"
#include "irq.h"

typedef struct {
    TimerPool *pool;
    Timer timer;
    IrqLine irq;

    uint8_t ctrl;
    uint64_t interval;
    uint64_t current;
    uint64_t start_time;
    bool pending;
} Pit;

void pit_init(Pit *pit, TimerPool *pool, IrqLine irq);
uint32_t pit_read(void *state, uint32_t offset, uint8_t size);
void pit_write(void *state, uint32_t offset, uint32_t value, uint8_t size);

#endif