#ifndef ICU_H
#define ICU_H

#include <stdint.h>
#include <stdbool.h>

#define ICU_IRR         0x00
#define ICU_ISR         0x04
#define ICU_IMR         0x08
#define ICU_PRIO        0x0C
#define ICU_VEC         0x2C
#define ICU_EOI         0x4C
#define ICU_DEFAULT     0x50
#define ICU_SIZE        0x54

typedef struct {
    uint32_t irr;
    uint32_t isr;
    uint32_t imr;
    uint32_t asserted;
    uint8_t prio[32];
    uint8_t vec[32];
    uint32_t def;
    bool has_int;
} Icu;

void icu_update_output_pin(Icu *icu);

void icu_raise_irq(Icu *icu, int irq);
void icu_lower_irq(Icu *icu, int irq);

int icu_next_irq(const Icu *icu);
int icu_accept_irq(Icu *icu, uint8_t *vector);
void icu_eoi(Icu *icu, uint32_t irq);

uint32_t icu_read(void *state, uint32_t offset, uint8_t size);
void icu_write(void *state, uint32_t offset, uint32_t value, uint8_t size);

#endif