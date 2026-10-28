#include "test.h"
#include "../emu/timer.h"

static void on_expiry(void *state) {
    unsigned *count = state;
    (*count)++;
}

TEST(timer_expires_at_deadline) {
    TimerPool pool = {0};
    Timer timer;
    unsigned count = 0;

    timer_init(&timer, on_expiry, &count);
    timer_mod(&pool, &timer, 100);

    pool.virtual_time = 99;
    timer_run_expired(&pool);
    CHECK(count == 0);

    pool.virtual_time = 100;
    timer_run_expired(&pool);
    CHECK(count == 1);
    CHECK(!timer.active);

    return true;
}

TEST(timer_can_be_cancelled) {
    TimerPool pool = {0};
    Timer timer;
    unsigned count = 0;

    timer_init(&timer, on_expiry, &count);
    timer_mod(&pool, &timer, 100);
    timer_del(&pool, &timer);

    pool.virtual_time = 100;
    timer_run_expired(&pool);

    CHECK(count == 0);
    CHECK(!timer.active);

    return true;
}