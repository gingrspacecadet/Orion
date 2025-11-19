#include "keyboard.h"

void kbd_write(Device* self, uint32_t addr, uint32_t value) {
    Keyboard* k = self->state;
    switch (addr) {
        case 0x2: {
            if (value == 1) {
                k->head = k->tail = k->count = 0;
                k->status = 0;
            }
            break;
        }
    }
}

uint32_t kbd_read(Device* self, uint32_t addr) {
    Keyboard* k = self->state;
    switch (addr) {
        case 0x0: {
            if (k->count == 0) return 0;
            uint8_t b = k->buffer[k->head];
            k->head = (k->head + 1) % sizeof(k->buffer);
            k->count--;
            k->status = (k->count > 0) ? 1 : 0;
            return b;
        }
        case 0x1: return k->status;
        default: return 0;
    }
}

void kbd_push(Device* self, char c) {
    Keyboard* k = self->state;
    if (k->count == sizeof(k->buffer)) return;
    k->buffer[k->tail] = c;
    k->tail = (k->tail + 1) % sizeof(k->buffer);
    k->count++;
    k->status = 1;
}