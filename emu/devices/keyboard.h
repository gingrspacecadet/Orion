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

Keyboard kbd_state = {
    .buffer = {0},
    .head = 0,
    .tail = 0,
    .count = 0,
    .status = 0,
};

Device kbd_device = (Device){
    .read = kbd_read,
    .write = kbd_write,
    .base = 0x00FF0000,
    .size = 0x2,
    .state = &kbd_state,
};

#endif