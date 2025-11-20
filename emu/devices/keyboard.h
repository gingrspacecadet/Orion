#ifndef DEVICES_KEYBOARD_H
#define DEVICES_KEYBOARD_H

#include "../device.h"

void kbd_write(Device* self, uint32_t addr, uint32_t value);

uint32_t kbd_read(Device* self, uint32_t addr);

void kbd_push(Device* self, char c);

typedef struct {
    uint8_t buffer[64];
    uint8_t head, tail, count;
    uint32_t status;
} Keyboard;

extern Keyboard kbd_state;
extern Device kbd_device;

#endif