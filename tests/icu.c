#include "test.h"
#include "../emu/icu.h"
#include "../emu/irq.h"

TEST(icu_pending_irq) {
    Icu icu = {0};
    IrqLine irq = icu_get_irq(&icu, 0);
    irq_raise(irq);
    CHECK(icu.asserted & (1u << 0));
    CHECK(icu.irr & (1u << 0));

    return true;
}

TEST(icu_masked_irq) {
    Icu icu = {0};
    IrqLine irq = icu_get_irq(&icu, 0);
    icu.imr |= 1u << 0;
    irq_raise(irq);
    CHECK(icu.asserted & (1u << 0));
    CHECK(icu.irr & (1u << 0));
    CHECK(icu_accept_irq(&icu, NULL) == -1);

    return true;
}

TEST(icu_unmask_pending_irq) {
    Icu icu = {0};
    IrqLine irq = icu_get_irq(&icu, 0);
    icu.imr |= 1u << 0;
    irq_raise(irq);
    CHECK(icu.asserted & (1u << 0));
    CHECK(icu.irr & (1u << 0));
    CHECK(icu_accept_irq(&icu, NULL) == -1);

    icu.imr &= ~(1u << 0);
    CHECK(icu_accept_irq(&icu, NULL) == 0);

    return true;
}

TEST(icu_priority_order) {
    Icu icu = {0};
    IrqLine irq = icu_get_irq(&icu, 0);
    IrqLine irq1 = icu_get_irq(&icu, 1);
    irq_raise(irq);
    irq_raise(irq1);
    CHECK(icu.asserted & (1u << 0) && icu.asserted & (1u << 1));
    CHECK(icu.irr & (1u << 0) && icu.irr & (1u << 1));
    CHECK(icu_accept_irq(&icu, NULL) == 0);
    icu_write(&icu, ICU_EOI, 0, 4);
    irq_lower(irq);
    CHECK(icu_accept_irq(&icu, NULL) == 1);

    return true;
}

TEST(icu_eoi_clears_isr) {
    Icu icu = {0};
    IrqLine irq = icu_get_irq(&icu, 0);
    irq_raise(irq);

    CHECK(icu_accept_irq(&icu, NULL) == 0);
    icu_write(&icu, ICU_EOI, 0, 4);
    CHECK((icu.imr & (1u << 0)) == 0);

    return true;
}

TEST(icu_level_repend) {
    Icu icu = {0};
    IrqLine irq = icu_get_irq(&icu, 0);
    irq_raise(irq);
    CHECK(icu_accept_irq(&icu, NULL) == 0);
    icu_write(&icu, ICU_EOI, 0, 4);
    CHECK(icu_accept_irq(&icu, NULL) == 0);

    return true;
}

TEST(icu_no_repend) {
    Icu icu = {0};
    IrqLine irq = icu_get_irq(&icu, 0);
    irq_raise(irq);
    CHECK(icu_accept_irq(&icu, NULL) == 0);
    icu_write(&icu, ICU_EOI, 0, 4);
    irq_lower(irq);
    CHECK(icu_accept_irq(&icu, NULL) == -1);

    return true;
}