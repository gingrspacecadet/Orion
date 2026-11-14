#include <stdio.h>
#include "uart.h"

void uart_write(void *state, uint32_t offset, uint32_t value, uint8_t size) {
    fputc(value, stderr);
}