#include "timer.h"
#include "irq.h"

typedef struct {
    TimerPool *pool;
    Timer timer;
    IrqLine irq;
    uint32_t interval_ns;
} HwTimer;
#include <stdio.h>
static void hw_timer_expired_cb(void *state) {
    HwTimer *timer = (HwTimer *)state;

    irq_raise(timer->irq);

    uint64_t next_deadline = timer->pool->virtual_time + timer->interval_ns;
    timer_mod(timer->pool, &timer->timer, next_deadline);
}

void hw_timer_init(HwTimer *timer, TimerPool *pool, IrqLine irq);