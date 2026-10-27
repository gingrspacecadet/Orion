#include <assert.h>
#include <stddef.h>
#include "timer.h"

void timer_mod(TimerPool *pool, Timer *timer, uint64_t expire_time) {
    assert(pool); assert(timer);
    
    if (timer->active)
        timer_del(pool, timer);

    timer->expire_time = expire_time;
    timer->active = true;

    Timer **curr = &pool->active_timers;
    while (*curr && (*curr)->expire_time <= expire_time) {
        curr = &(*curr)->next;
    }

    timer->next = *curr;
    *curr = timer;
}

void timer_del(TimerPool *pool, Timer *timer) {
    assert(pool); assert(timer);

    if (!timer->active) return;

    Timer **curr = &pool->active_timers;
    while (*curr) {
        if (*curr == timer) {
            *curr = timer->next;
            timer->active = false;
            timer->next = NULL;
            return;
        }

        curr = &(*curr)->next;
    }
}

void timer_run_expired(TimerPool *pool) {
    assert(pool);
    while (pool->active_timers && pool->virtual_time >= pool->active_timers->expire_time) {
        Timer *timer = pool->active_timers;

        pool->active_timers = timer->next;
        timer->active = false;
        timer->next = NULL;

        if (timer->cb)
            timer->cb(timer->state);
    }
}
