#include "hw_timer.h"

void hw_timer_init(HwTimer *timer, TimerPool *pool, IrqLine irq) {
    timer->pool = pool;
    timer->irq = irq;
    timer->interval_ns = 1000000;

    timer_init(&timer->timer, hw_timer_expired_cb, timer);
    timer_mod(pool, &timer->timer, pool->virtual_time + timer->interval_ns);
}