#ifndef IRQ_H
#define IRQ_H

#include <stdbool.h>

typedef void (*irq_set_level_fn)(void *state, int pin, bool level);

typedef struct {
    irq_set_level_fn set_level;
    void *state;
    int pin;
} IrqLine;

static inline void irq_set(IrqLine line, bool level) {
    if (line.set_level) {
        line.set_level(line.state, line.pin, level);
    }
}

static inline void irq_raise(IrqLine line) { irq_set(line, true); }
static inline void irq_lower(IrqLine line) { irq_set(line, false); }

#endif