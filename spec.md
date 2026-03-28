# Orion design spec

registers  
R0..R15 general-purpose  
R15 is SP  
Private PC and flags registers  

SP and PC are both stored as byte addresses, but must be word-aligned.

PC always points to the next instruction to be processed.  

the Flags register - a private register containing all the status flags of the cpu.
|Bit|Flag name|Desc|
|---|---------|----|
|0|Carry (C)|Set when the last alu operation had an arithmetic carry|
|1|Overflow (V)|Set when the last alu operation's result overflows a int32_t|
|2|Zero (Z)|Set when the last alu operation resulted in exactly 0|
|3|Negative (N)|Set when the last alu operation's result's MSB was set|
|4|Interrupts Enabled(IE)|Set manually using the `INTE` instruction. When false, external interrupts are ignored|

fixed 32-bit instructions, with optional extended immediates for funsies  

little endian  

basic alu, might add more later  

un/conditional JMP, CALL/RET push and pop from the stack, all relative offsets in bytes  

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

J-type: opcode(6) reserved(3) cond(4) (rm(4) | imm(16)) absolute?(1) register?(1) extended?(1)  
A-type: opcode(6) rn(4) rd(4) (rm(4) | imm(16)) register?(1) extended?(1)  
M-type: opcode(6) rn(4) rd(4) (rm(4) | imm(16)) register?(1) extended?(1)  
F-type: opcode(6) enabled?(1) reserved(25)

J-type decoding:  
If `absolute?` is set, treat rm or imm (depending on `register?`) as an absolute jump (`PC = addr`), else treat like relative (`PC += offset`)

M-type meaning:  
   R\[rd\] = ram\[rn + offset\];
   `rn` is the base register, `rm` or `imm` are the offsets on top of that.

all `reserved` bits **MUST** be 0. If not, it raises an "Invalid Instruction" fault.

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
|0x16  |CALL    |J   |Pushes the byte address of the next instruction and jumps to addr|
|0x17  |RET     |J   |Pops into PC  |
|0x18  |PUSH    |M   |If register mode, pushes specified `rm`. Otherwise, treats `imm` as a bitmask of registers to push in ascending order. Stores at `SP`, then decrements by 4|
|0x19  |POP     |M   |If register mode, pops specified `rm`. Otherwise, treats `imm` as a bitmask of registers to pop in descending order. Increments by 4, then loads from `SP`|
|0x1A-1F|reserved|||
|0x20  |INTE    |F   |Sets the `IE` flag to `enabled`|
|0x21-3F|reserved|||

J-type conditions:
assemblers should prefer using these mnemonics, and encode `cond` accordingly  
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


Exact opcode spec table

|Mnem|Flag updates|PC effect|SP effect|Fault cases|
|----|------------|---------|---------|-----------|
|SUB |C,V,Z,N     |         |         |           |
|ADD |C,V,Z,N     |         |         |           |
|MUL |C,V,Z,N     |         |         |           |
|DIV |C,V,Z,N     |         |         |Division by 0|
|SHL |C,V,Z,N     |         |         |           |
|SHR |C,V,Z,N     |         |         |           |
|AND |Z,N         |         |         |           |
|OR  |Z,N         |         |         |           |
|NOT |Z,N         |         |         |           |
|XOR |Z,N         |         |         |           |
|LDR |            |         |         |Out-of-bounds target|
|STR |            |         |         |Out-of-bounds target|
|LDRB|            |         |         |Out-of-bounds target|
|STRB|            |         |         |Out-of-bounds target|
|JXX |            |Sets to decoded target if `cond` is true|         |Out-of-bounds target|
|CALL|            |Pushes PC + 4 to SP, then follows `JXX` logic to jump to target unconditionally|Decrements by 4|Out-of-bounds target|
|RET |            |Pops from SP|Increments by 4|Out-of-bounds target|
|PUSH|            |         |On single-register, decrements by 4. On bitmask, decrements by 4 for every set bit|Pushing SP|
|POP |            |         |On single-register, increments by 4. On bitmask, increments by 4 for every set bit|Popping SP|
|INTE|IE          |         |         |Any `reserved` bits set|

CPU Exceptions:
|Name|Number|Description|
|----|------|-----------|
|Invalid instruction|0x0|A general error thrown by the decoder if it fails to properly decode an instruction|
|Misaligned PC|0x1|Thrown when PC is not a multiple of 4|
|Invalid memory access|0x2|Thrown when an instruction attempts to access a memory location that does not exist|
|Stack under/overflow|0x3|Thrown when trying to pop past memory maximum or push past 0x0|
|reserved|0x4-0x1F||
|Interrupt entry|0x20-0xFF|Not a cpu exception, and infact a hardware interrupt|

For all exceptions less than `0x20`, the cpu first pushes some details to the stack, then jumps to the address stored in the Interrupt Handler Vector Table: `PC = IHVT[irq]`. For "Interrupt Entry" exceptions, the `IE` flag must be enabled for this to happen, otherwise the exception is ignored.

The IHVT is a 256-word-long array of function pointers. Its address is `0x0000_0100`.

On an exception, the cpu pushes the following:
- PC
- the offending instruction (if any)