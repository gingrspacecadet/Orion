#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include "machine.h"

uint32_t bus_read(Machine* m, uint32_t addr);

void bus_write(Machine* m, uint32_t addr, uint32_t value);

#endif