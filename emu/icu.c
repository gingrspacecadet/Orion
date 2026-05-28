#include "icu.h"

static void icu_update_output_pin(IcuDevice *icu) {
    uint32_t active_requests = icu->irr & ~icu->imr;
    *icu->cpu_int_pin = (active_requests != 0);
}

void icu_raise_irq(IcuDevice *icu, int irq_num) {
    if (irq_num >= 0 && irq_num < 32) {
        icu->irr |= (1u << irq_num);
        icu_update_output_pin(icu);
    }
}

void icu_lower_irq(IcuDevice *icu, int irq_num) {
    if (irq_num >= 0 && irq_num < 32) {
        icu->irr &= ~(1u << irq_num);
        icu_update_output_pin(icu);
    }
}

uint32_t icu_read(void *state, uint32_t offset, uint8_t size) {
    IcuDevice *icu = (IcuDevice *)state;

    switch (offset) {
        case ICU_IRR: return icu->irr;
        case ICU_ISR: return icu->isr;
        case ICU_IMR: return icu->imr;
        default: /* raise_interrupt */ return 0;
    }
}

void icu_write(void *state, uint32_t offset, uint32_t value, uint8_t size) {
    IcuDevice *icu = (IcuDevice *)state;

    if (offset == ICU_IMR) {
        icu->imr = value;
        icu_update_output_pin(icu);
    }
    else if (offset >= ICU_PRIO && offset < ICU_VEC) {
        uint32_t idx = offset - ICU_PRIO;
        if (idx < 32) icu->prio[idx] = value & 0xFF;
    }
    else if (offset >= ICU_VEC && offset < ICU_EOI) {
        uint32_t idx = offset - ICU_VEC;
        if (idx < 32) icu->vec[idx] = value & 0xFF;
    }
    else if (offset == ICU_EOI) {
        uint8_t irq_to_clear = value & 0x1F;
        icu->isr &= ~(1u << irq_to_clear);
        icu_update_output_pin(icu);
    }
    else if (offset == ICU_DEF) {
        icu->def = value;
    }
    else {
        // TODO: throw exception
    }
}

static void icu_set_irq_line_level(void *state, int pin, bool level) {
    IcuDevice *icu = (IcuDevice *)state;
    if (level) {
        icu_raise_irq(icu, pin);
    } else {
        icu_lower_irq(icu, pin);
    }
}

IrqLine icu_get_irq_line(IcuDevice *icu, int irq_num) {
    IrqLine line = {0};
    if (irq_num >= 0 && irq_num < 32) {
        line.set_level = icu_set_irq_line_level;
        line.state = icu;
        line.pin = irq_num;
    }
    return line;
}