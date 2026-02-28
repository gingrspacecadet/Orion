#ifndef HANDLERS_H
#define HANDLERS_H

#include <machine.h>

typedef void (*OpcodeHandler)(Machine *);

#define OP(name) void name(Machine *machine)

OP(NOP);

extern OpcodeHandler jumptable[];

#endif