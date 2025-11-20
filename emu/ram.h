#ifndef RAM_H
#define RAM_H

#include <stdint.h>
#include "machine.h"

void ram_init(Machine* m);
void ram_free(void);
void ram_write(uint32_t addr, uint32_t value);
uint32_t ram_read(uint32_t addr);

#endif
