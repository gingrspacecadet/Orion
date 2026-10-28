#ifndef IRQ_H
#define IRQ_H

#include <assert.h>
#include <stdbool.h>
#include "icu.h"

typedef struct {
    Icu *icu;
    uint32_t pin;
} IrqLine;

static inline void irq_set(IrqLine line, bool level) {
    if (level)
        icu_raise_irq(line.icu, line.pin);
    else
        icu_lower_irq(line.icu, line.pin);
}

static inline void irq_raise(IrqLine line) {
    irq_set(line, true);
}

static inline void irq_lower(IrqLine line) {
    irq_set(line, false);
}

static inline IrqLine icu_get_irq(Icu *icu, int irq) {
    assert(icu);
    assert(irq >= 0 && irq < 32);

    return (IrqLine) {
        .icu = icu,
        .pin = irq,
    };
}

#endif