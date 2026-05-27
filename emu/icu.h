#ifndef ICU_H
#define ICU_H

#include <stdint.h>
#include <stdbool.h>

#define ICU_REG_ISR     0x00
#define ICU_REG_IER     0x04
#define ICU_REG_IACK    0x08

typedef struct {
    uint32_t isr;
    uint32_t ier;
    bool *cpu_int_pin;
} IcuDevice;

void icu_raise_irq(IcuDevice *icu, int irq_num);
void icu_lower_irq(IcuDevice *icu, int irq_num);

uint32_t icu_read(void *state, uint32_t offset, uint8_t size);
void icu_write(void *state, uint32_t offset, uint32_t value, uint8_t size);

#endif