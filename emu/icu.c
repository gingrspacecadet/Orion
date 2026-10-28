#include <assert.h>
#include "icu.h"

static uint32_t irq_bit(int irq) {
    return UINT32_C(1) << irq;
}

void icu_update_output_pin(Icu *icu) {
    assert(icu);
    
    icu->has_int = icu_next_irq(icu) >= 0;
}

void icu_raise_irq(Icu *icu, int irq) {
    assert(icu); assert(irq >= 0 && irq < 32);

    uint32_t bit = irq_bit(irq);

    icu->asserted |= bit;

    if (!(icu->isr & bit))
        icu->irr |= bit;

    icu_update_output_pin(icu);
}

void icu_lower_irq(Icu *icu, int irq) {
    if (!icu || irq < 0 || irq >= 32)
        return;

    uint32_t bit = irq_bit(irq);

    icu->asserted &= ~bit;
    icu->irr &= ~bit;

    icu_update_output_pin(icu);
}

int icu_next_irq(const Icu *icu) {
    if (!icu)
        return -1;

    int active_priority = -1;

    for (int i = 0; i < 32; i++) {
        if ((icu->isr & irq_bit(i)) && (int)icu->prio[i] > active_priority) {
            active_priority = icu->prio[i];
        }
    }

    uint32_t pending = icu->irr & ~icu->imr & ~icu->isr;

    int selected = -1;

    for (int i = 0; i < 32; i++) {
        if (!(pending & irq_bit(i)))
            continue;

        if ((int)icu->prio[i] <= active_priority)
            continue;

        if (selected < 0 || icu->prio[i] > icu->prio[selected]) {
            selected = i;
        }
    }

    return selected;
}

int icu_accept_irq(Icu *icu, uint8_t *vector) {
    int irq = icu_next_irq(icu);

    if (irq < 0)
        return -1;

    uint32_t bit = irq_bit(irq);

    icu->irr &= ~bit;
    icu->isr |= bit;

    uint8_t selected_vector = icu->vec[irq];

    if (selected_vector < 0x20 || selected_vector == 0xFF)
        selected_vector = (uint8_t)icu->def;

    if (vector)
        *vector = selected_vector;

    icu_update_output_pin(icu);

    return irq;
}

void icu_eoi(Icu *icu, uint32_t irq) {
    if (!icu || irq >= 32)
        return;

    uint32_t bit = irq_bit((int)irq);

    icu->isr &= ~bit;

    if (icu->asserted & bit)
        icu->irr |= bit;
    else
        icu->irr &= ~bit;

    icu_update_output_pin(icu);
}

uint32_t icu_read(void *state, uint32_t offset, uint8_t size) {
    assert(state);

    Icu *icu = state;

    if (size == 4) {
        switch (offset) {
            case ICU_IRR:     return icu->irr;
            case ICU_ISR:     return icu->isr;
            case ICU_IMR:     return icu->imr;
            case ICU_DEFAULT: return icu->def;
            default:          return 0;
        }
    }

    if (size == 1) {
        if (offset >= ICU_PRIO && offset < ICU_VEC)
            return icu->prio[offset - ICU_PRIO];

        if (offset >= ICU_VEC && offset < ICU_EOI)
            return icu->vec[offset - ICU_VEC];
    }

    return 0;
}

void icu_write(void *state, uint32_t offset, uint32_t value, uint8_t size) {
    assert(state);
    
    Icu *icu = state;

    if (size == 4) {
        switch (offset) {
            case ICU_IMR:
                icu->imr = value;
                icu_update_output_pin(icu);
                return;

            case ICU_EOI:
                icu_eoi(icu, value);
                return;

            case ICU_DEFAULT:
                icu->def = value;
                return;

            default:
                return;
        }
    }

    if (size == 1) {
        if (offset >= ICU_PRIO && offset < ICU_VEC) {
            icu->prio[offset - ICU_PRIO] = (uint8_t)value;
            icu_update_output_pin(icu);
            return;
        }

        if (offset >= ICU_VEC && offset < ICU_EOI) {
            icu->vec[offset - ICU_VEC] = (uint8_t)value;
            return;
        }
    }
}