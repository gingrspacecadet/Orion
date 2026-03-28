# Orion design spec

registers  
R0..R15 general-purpose  
R15 is SP  
Private PC and flags registers  

PC counts in words, and always points to the current instruction being processed.  

the Flags register - a private register containing all the status flags of the cpu.
|Bit|Flag name|Desc|
|---|---------|----|
|0|Carry (C)|Set when the last alu operation's result couldn't fit into 32 bits, but can in 33 (aka this bit)|
|1|Overflow (V)|Set when the last alu operation's result overflows a int32_t|
|2|Zero (Z)|Set when the last alu operation resulted in exactly 0|
|3|Negative (N)|Set when the last alu operation's result's MSB was set|
|4|Interrupts Enabled(IE)|Set manually using the `INTE` instruction. When false, external interrupts are ignored|

fixed 32-bit instructions, with optional extended immediates for funsies  

little endian  

basic alu, might add more later  

un/conditional JMP, CALL/RET push and pop from the stack, all relative offsets in words  

Stack - SP points to next free section of memory. Stack grows downwards.

Memory access - memory is byte-addressable, and the ISA provides methods for both bytes and words.

fancy immediates!  
if enabled by the extended flag in the instruction, the next word is treated as the immediate, allowing for true 32-bit immediates!  
if the bit is set, the program counter will be incremented by a further word by the decoder.

encoding types  
J-type: JMP, JE, JGE, CALL, etc  
A-type: ADD, SUB, SHL etc  
M-type: LDR, STR,  

A-types always update flags as they go through the ALU, which handles it.  

J-type: opcode(6) rn(4) mode(4) (rm(4) | imm(16)) register?(1) extended?(1)  
A-type: opcode(6) rn(4) rd(4) (rm(4) | imm(16)) register?(1) extended?(1)  
M-type: opcode(6) rn(4) (rm(4) | imm(16)) register?(1) extended?(1)  

Instructions:
|Number|Mnemonic|Type|Description   |
|------|--------|----|--------------|
|0x0   |ADD     |A   |Addition      |
|0x1   |SUB     |A   |Subtraction   |
|0x2   |MUL     |A   |Multiplication|
|0x3   |DIV     |A   |Division      |
|0x4   |SHL     |A   |Left shift    |
|0x5   |SHR     |A   |Right shift   |
|0x6   |AND     |A   |bitwise and   |
|0x7   |OR      |A   |bitwise or    |
|0x8   |NOT     |A   |bitwise not   |
|0x9   |XOR     |A   |bitwise xor   |
|0x10  |reserved|    |              |
|0x11  |LDR     |M   |Load a word   |
|0x12  |STR     |M   |Store a word  |
|0x13  |LDRB    |M   |Load a byte   |
|0x14  |STRB    |M   |Store a byte  |
|0x15  |JXX     |J   |Jumps to addr |
|0x16  |CALL    |J   |Pushes PC and jumps to addr|
|0x17  |RET     |J   |Pops PC       |
|0x18  |PUSH    |M   |If register mode, pushes specified `rm`. Otherwise, treats `imm` as a bitmask of registers to push. Stores at `SP`, then decrements by 4|
|0x19  |POP     |M   |If register mode, pops specified `rm`. Otherwise, treats `imm` as a bitmask of registers to pop. Loads from `SP`, then increments by 4|
|0x1A-1F|reserved|||
|0x20  |INTE    |F   |Toggles the `INTE` flag based on the immediate|
|0x21-3F|reserved|||
|0x40  |reserved|    |              |

J-type modes:
assemblers should prefer using these mnemonics, and encoding `mode` accordingly  
|num|mnem|condition|
|---|----|---------|
|0x0|JMP|`true`|
|0x1|JEQ|`Z == 1`|
|0x2|JNE|`Z == 0`|
|0x3|JLT|`N != V`|
|0x4|JGE|`N == V`|
|0x5|JLTU|`C == 0`|
|0x6|JGEU|`C == 1`|
|0x7|JCS|`C == 1`|
|0x8|JCC|`C == 0`|
|0x9|JN|`N == 1`|
|0xA|JP|`N == 0`|
|0xB|JVS|`V == 1`|
|0xC|JVC|`V == 0`|
|0xD|JHI|`(C == 1) && (Z == 0)`|
|0xE|JLS|`C == 0`|
|0xF|reserved||

