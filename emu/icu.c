#include "icu.h"

static void icu_update_output_pin(IcuDevice *icu) {
    *icu->cpu_int_pin = icu->isr && icu->ier;
}

void icu_raise_irq(IcuDevice *icu, int irq_num) {
    if (irq_num >= 0 && irq_num < 32) {
        icu->isr |= (1u << irq_num);
        icu_update_output_pin(icu);
    }
}

void icu_lower_irq(IcuDevice *icu, int irq_num) {
    if (irq_num >= 0 && irq_num < 32) {
        icu->isr &= ~(1u << irq_num);
        icu_update_output_pin(icu);
    }
}

uint32_t icu_read(void *state, uint32_t offset, uint8_t size) {
    IcuDevice *icu = (IcuDevice *)state;

    switch (offset) {
        case ICU_REG_ISR: return icu->isr;
        case ICU_REG_IER: return icu->ier;
        case ICU_REG_IACK: {
            uint32_t active = icu->isr & icu->ier;
            if (active == 0) return 0xFFFFFFFF;

            for (int i = 0; i < 32; i++) {
                if (active & (1u << i)) return i;
            }
            return 0xFFFFFFFF;
        }
        default: return 0;
    }
}

void icu_write(void *state, uint32_t offset, uint32_t value, uint8_t size) {
    IcuDevice *icu = (IcuDevice *)state;

    switch (offset) {
        case ICU_REG_IER: {
            icu->ier = value;
            icu_update_output_pin(icu);
            break;
        }
        case ICU_REG_IACK: {
            if (value < 32) {
                icu->isr &= ~(1u << value);
                icu_update_output_pin(icu);
            }
            break;
        }
    }
}