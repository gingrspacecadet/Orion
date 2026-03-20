# Orion design spec

registers  
R0..R15  
R15 is SP  

pc aligned to 32-bit word  

fixed 32-bit instructions, with optional variable width immediates for funsies  

big endian  

basic alu, might add more later  

un/conditional JMP, CALL/RET push and pop from the stack, all relative offsets  

Memory access - Byte and word accessing. The LDR/STR ops can do both  

fancy immediates!  
if enabled by the VariableImmediates flag in the instruction, the next word is treated as the immediate, allowing for true 32-bit immediates!  

encoding types  
J-type: JMP, JE, JGE, CALL, etc  
A-type: ADD, SUB, SHL etc  
M-type: LDR, STR,  

J-type: opcode(6) imm(24) signed?(1) extended?(1)  
A-type: opcode(6) rn(4) rm(4) imm(16) signed?(1) extended?(1)  
M-type: opcode(6) rn(4) rm(4) imm(16) signed?(1) extended?(1)  