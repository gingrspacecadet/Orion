# Orion design spec

registers  
R0..R15 general-purpose  
R15 is SP  

pc aligned to 32-bit word  

fixed 32-bit instructions, with optional extended immediates for funsies  

big endian  

basic alu, might add more later  

un/conditional JMP, CALL/RET push and pop from the stack, all relative offsets  

Memory access - Byte accessing.

fancy immediates!  
if enabled by the extended flag in the instruction, the next word is treated as the immediate, allowing for true 32-bit immediates!  

encoding types  
J-type: JMP, JE, JGE, CALL, etc  
A-type: ADD, SUB, SHL etc  
M-type: LDR, STR,  


J-type: opcode(6) base(4) roffset(4) ioffset(16) register?(1) extended?(1)  
A-type: opcode(6) rn(4) rd(4) (rm(4) | imm(16)) register?(1) extended?(1)  
M-type: opcode(6) base(4) roffset(4) ioffset(16) register?(1) extended?(1)  