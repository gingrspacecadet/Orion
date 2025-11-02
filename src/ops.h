#ifndef OPS_H
#define OPS_H

#include "machine.h"

OP(LDI);
OP(HLT);
OP(ADD);
OP(SUB);
OP(MUL);
OP(DIV);
OP(MOD);
OP(CMP);
OP(JZ);
OP(JNZ);
OP(JMP);
OP(PUSH);
OP(POP);
OP(CALL);
OP(RET);
OP(MOV);
OP(NOP);
OP(IRET);

#endif