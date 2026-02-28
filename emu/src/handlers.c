#include <handlers.h>

OpcodeHandler jumptable[] = {
    [0x0] = NOP,
};

OP(NOP) {
    return;
}