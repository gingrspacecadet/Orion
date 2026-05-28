#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*timer_fn)(void *state);

typedef struct Timer {
    uint64_t expire_time;
    timer_fn cb;
    void *state;
    bool active;
    struct Timer *next;
} Timer;

typedef struct {
    uint64_t virtual_time;
    Timer *active_timers;
} TimerPool;

void timer_init(Timer *timer, timer_fn cb, void *state);
void timer_mod(TimerPool *pool, Timer *timer, uint64_t expire_time);
void timer_del(TimerPool *pool, Timer *timer);
void timer_run_expired(TimerPool *pool);

#endif