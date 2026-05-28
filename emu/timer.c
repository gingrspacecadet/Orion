#include <stdlib.h>
#include "timer.h"

void timer_init(Timer *timer, timer_fn cb, void *state) {
    timer->expire_time = 0;
    timer->cb = cb;
    timer->state = state;
    timer->active = false;
    timer->next = NULL;
}

void timer_mod(TimerPool *pool, Timer *timer, uint64_t expire_time) {
    if (timer->active) {
        timer_del(pool, timer);
    }

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
    while (pool->active_timers && pool->virtual_time >= pool->active_timers->expire_time) {
        Timer *timer = pool->active_timers;

        pool->active_timers = timer->next;
        timer->active = false;
        timer->next = NULL;
        if (timer->cb) {
            timer->cb(timer->state);
        }
    }
}