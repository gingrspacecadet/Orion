#include "pit.h"
#include "isa.h"
#include <stdio.h>
static void expired_cb(void *state) {
    Pit *pit = state;

    if (pit->ctrl & PIT_CTRL_INT) {
        irq_raise(pit->irq);
    }

    if ((pit->ctrl & PIT_CTRL_AUTO) && (pit->ctrl & PIT_CTRL_ENABLE)) {
        uint64_t next_deadline = pit->pool->virtual_time + pit->interval;
        timer_mod(pit->pool, &pit->timer, next_deadline);
    }
}

void pit_init(Pit *pit, TimerPool *pool, IrqLine irq) {
    pit->pool = pool;
    pit->irq = irq;
    pit->interval = 1000000; // purely for testing

    timer_init(&pit->timer, expired_cb, pit);
    timer_mod(pool, &pit->timer, pool->virtual_time + pit->interval); // this too
}