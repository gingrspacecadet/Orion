#ifndef ICU_H
#define ICU_H

#include <stdint.h>
#include <stdbool.h>
#include "irq.h"

#define ICU_IRR     0x00
#define ICU_ISR     0x04
#define ICU_IMR     0x08
#define ICU_PRIO    0x0C
#define ICU_VEC     0x2C
#define ICU_EOI     0x4C
#define ICU_DEF     0x50

typedef struct {
    uint32_t irr;
    uint32_t isr;
    uint32_t imr;
    uint8_t prio[32];
    uint8_t vec[32];
    uint32_t eoi;
    uint32_t def;
    bool *cpu_int_pin;
} IcuDevice;

void icu_raise_irq(IcuDevice *icu, int irq_num);
void icu_lower_irq(IcuDevice *icu, int irq_num);

uint32_t icu_read(void *state, uint32_t offset, uint8_t size);
void icu_write(void *state, uint32_t offset, uint32_t value, uint8_t size);

IrqLine icu_get_irq_line(IcuDevice *icu, int irq_num);

static void icu_update_output_pin(IcuDevice *icu) {
    uint32_t active_requests = icu->irr & ~icu->imr;
    *icu->cpu_int_pin = (active_requests != 0);
}

#endif