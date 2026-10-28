#include <assert.h>
#include <stdint.h>

#include "pit.h"
#include "isa.h"

static void pit_update_irq(Pit *pit) {
    assert(pit);

    if (pit->pending && (pit->ctrl & PIT_CTRL_INT)) {
        irq_raise(pit->irq);
    } else {
        irq_lower(pit->irq);
    }
}

static uint64_t pit_current_value(const Pit *pit) {
    assert(pit);
    
    if (!(pit->ctrl & PIT_CTRL_ENABLE)) {
        return pit->current;
    }

    uint64_t now = pit->pool->virtual_time;

    if (now < pit->start_time) {
        return 0;
    }

    uint64_t elapsed = now - pit->start_time;

    if (pit->interval == 0) {
        return elapsed;
    }

    if (!pit->timer.active) {
        return pit->current;
    }

    if (pit->ctrl & PIT_CTRL_AUTO) {
        return elapsed % pit->interval;
    }

    return elapsed < pit->interval ? elapsed : pit->interval;
}

static void pit_start_interval(Pit *pit) {
    assert(pit);
    timer_del(pit->pool, &pit->timer);

    pit->current = 0;
    pit->start_time = pit->pool->virtual_time;

    if (pit->interval == 0) {
        return;
    }

    if (pit->interval > UINT64_MAX - pit->start_time) {
        return;
    }

    timer_mod(pit->pool, &pit->timer, pit->start_time + pit->interval);
}


static bool pit_next_deadline(Pit *pit, uint64_t previous_deadline, uint64_t *out) {
    assert(pit); assert(out);
    uint64_t interval = pit->interval;

    if (interval == 0 || interval > UINT64_MAX - previous_deadline) {
        return false;
    }

    uint64_t deadline = previous_deadline + interval;
    uint64_t now = pit->pool->virtual_time;

    if (deadline <= now) {
        uint64_t skipped = (now - deadline) / interval + 1;

        if (skipped > (UINT64_MAX - deadline) / interval) {
            return false;
        }

        deadline += skipped * interval;
    }

    *out = deadline;
    return true;
}

static void expired_cb(void *state) {
    assert(state);

    Pit *pit = state;

    if (!(pit->ctrl & PIT_CTRL_ENABLE) || pit->interval == 0) {
        return;
    }

    uint64_t expired_deadline = pit->timer.expire_time;

    pit->pending = true;
    pit->current = pit->interval;

    pit_update_irq(pit);

    if (!(pit->ctrl & PIT_CTRL_AUTO)) {
        return;
    }

    uint64_t next_deadline;

    if (!pit_next_deadline(pit, expired_deadline, &next_deadline)) {
        pit->current = pit->interval;
        return;
    }

    pit->start_time = next_deadline - pit->interval;
    pit->current = 0;

    timer_mod(pit->pool, &pit->timer, next_deadline);
}

void pit_init(Pit *pit, TimerPool *pool, IrqLine irq) {
    assert(pit);
    assert(pool);

    *pit = (Pit){0};

    pit->pool = pool;
    pit->irq = irq;
    pit->start_time = pool->virtual_time;

    timer_init(&pit->timer, expired_cb, pit);

    pit_update_irq(pit);
}

void pit_write(void *state, uint32_t offset, uint32_t value, uint8_t size) {
    assert(state);

    Pit *pit = state;

    if (size != 4) {
        return;
    }

    switch (offset) {
        case PIT_CTRL: {
            uint8_t new_ctrl = (uint8_t)value & (PIT_CTRL_ENABLE | PIT_CTRL_AUTO | PIT_CTRL_INT);

            bool was_enabled = (pit->ctrl & PIT_CTRL_ENABLE) != 0;
            bool will_enable = (new_ctrl & PIT_CTRL_ENABLE) != 0;

            if (was_enabled && !will_enable) {
                pit->current = pit_current_value(pit);
                timer_del(pit->pool, &pit->timer);

                pit->ctrl = new_ctrl;
                pit_update_irq(pit);
                return;
            }

            pit->ctrl = new_ctrl;

            if (!was_enabled && will_enable) {
                pit_start_interval(pit);
            }

            pit_update_irq(pit);
            return;
        }

        case PIT_INT_LO: {
            pit->interval = (pit->interval & (UINT64_MAX << 32)) | (uint64_t)value;

            if (pit->ctrl & PIT_CTRL_ENABLE) {
                pit_start_interval(pit);
            }
            return;
        }

        case PIT_INT_HI: {
            pit->interval = ((uint64_t)value << 32) | (pit->interval & UINT32_MAX);

            if (pit->ctrl & PIT_CTRL_ENABLE) {
                pit_start_interval(pit);
            }
            return;
        }

        case PIT_ACK: {
            pit->pending = false;
            pit_update_irq(pit);
            return;
        }

        default:
            return;
    }
}

uint32_t pit_read(void *state, uint32_t offset, uint8_t size) {
    assert(state);

    Pit *pit = state;

    if (size != 4) {
        return 0;
    }

    switch (offset) {
        case PIT_CTRL:
            return pit->ctrl;

        case PIT_INT_LO:
            return (uint32_t)pit->interval;

        case PIT_INT_HI:
            return (uint32_t)(pit->interval >> 32);

        case PIT_CUR_LO: {
            uint64_t current = pit_current_value(pit);
            return (uint32_t)current;
        }

        case PIT_CUR_HI: {
            uint64_t current = pit_current_value(pit);
            return (uint32_t)(current >> 32);
        }

        default:
            return 0;
    }
}