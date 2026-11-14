#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_write(void *state, uint32_t offset, uint32_t value, uint8_t size);

#endif