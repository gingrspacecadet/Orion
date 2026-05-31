# Orion design spec

## Registers

- `R0..R14` general-purpose
- `R15` is `SP`
- Private `PC` and flags registers

`SP` and `PC` are both stored as byte addresses, but must be word-aligned.

`PC` always points to the current instruction being executed.

---

## Flags register

The Flags register is a private register containing all CPU status flags.

| Bit | Flag name | Desc |
|---|---------|----|
| 0 | Carry (`C`) | Set when the last ALU operation had an arithmetic carry |
| 1 | Overflow (`V`) | Set when the last ALU operation result overflows a signed 32-bit integer |
| 2 | Zero (`Z`) | Set when the last ALU operation resulted in exactly `0` |
| 3 | Negative (`N`) | Set when the last ALU operation result MSB was set |
| 4 | Interrupts Enabled (`IE`) | Set manually. When false, external interrupts are ignored |

---

## Instruction properties

- Fixed 32-bit instructions
- Little endian
- Basic ALU, with possible future expansion
- Unconditional and conditional `JMP`, `CALL`, `RET`
- Stack push and pop operations
- All relative offsets are in bytes
- Stack grows downwards
- `SP` points to next free section of memory
- Memory is byte-addressable
- ISA provides methods for both bytes and words

---

## Encoding types

- `J-type`: `JMP`, `JE`, `JGE`, `CALL`, etc
- `A-type`: `ADD`, `SUB`, `SHL`, etc
- `M-type`: `LDR`, `STR`
- `S-type`: `FLAGS`

`A-type` instructions always update flags via the ALU.

---

## Instruction formats

`J-type`:
- `opcode(6)` `cond(4)` `absolute?(1)` `reserved(3)` `(rm(4) | imm(16))` `register?(1)` `signed?(1)`

`A-type`:
- `opcode(6)` `rn(4)` `rd(4)` `(rm(4) | imm(16))` `register?(1)` `signed?(1)`

`M-type`:
- `opcode(6)` `rn(4)` `rd(4)` `(rm(4) | imm(16))` `register?(1)` `signed?(1)`

`S-type`:
- specified per instruction

`J-type` decoding:
- If `absolute?` is set, treat `rm` or `imm` as an absolute jump: `PC = addr`
- Otherwise, treat it as relative: `PC += offset`

`M-type` meaning:
- `R[rd] = ram[rn + offset]`
- `rn` is the base register
- `rm` or `imm` are offsets on top of that

All `reserved` bits must be `0`. If not, it raises an `Invalid Instruction` fault.

---

## Instructions

| Number | Mnemonic | Type | Description |
|------|--------|----|--------------|
| `0x00` | reserved | | Exists to cause a fault when executing uninitialized code |
| `0x01` | ADD | A | Addition |
| `0x02` | SUB | A | Subtraction |
| `0x03` | MUL | A | Multiplication |
| `0x04` | DIV | A | Division |
| `0x05` | SHL | A | Left shift |
| `0x06` | SHR | A | Right shift |
| `0x07` | AND | A | Bitwise and |
| `0x08` | OR | A | Bitwise or |
| `0x09` | NOT | A | Bitwise not (ignores `rn`) |
| `0x0A` | XOR | A | Bitwise xor |
| `0x0B` | LUI | A | Loads `rm`/`imm` into top 16 bits of `rd` (ignores `rn`) |
| `0x0C` | CMP | A | Subtracts and discards result (`rd` is ignored) |
| `0x0D` | LDR | M | Load a word |
| `0x0E` | STR | M | Store a word |
| `0x0F` | LDRB | M | Load a byte |
| `0x10` | STRB | M | Store a byte |
| `0x11` | JXX | J | Jumps to addr |
| `0x12` | CALL | J | Pushes PC and jumps to addr |
| `0x13` | RET | J | Pops PC |
| `0x14` | PUSH | M | If register mode, pushes specified `rm`. Otherwise uses `imm` as a register bitmask and pushes in ascending order. Decrements by 4, then stores at `SP` |
| `0x15` | POP | M | If register mode, pops specified `rm`. Otherwise uses `imm` as a register bitmask and pops in descending order. Loads from `SP`, then increments |
| `0x16-20` | reserved | | |
| `0x21` | FLAGS | S | Reads `flags` to `rd` or writes `rm`/`imm` to `flags` |
| `0x22` | HALT | x | Pauses the CPU until an interrupt fires |
| `0x23` | ICALL | J | Calls the specified interrupt number in `rm` or `imm` |
| `0x24` | IRET | J | Pops `FLAGS` then `PC` |
| `0x25-3F` | reserved | | |

---

## `FLAGS` encoding

- `opcode(6)` `(rd(4) | (rm(4) | imm(16)))` `reserved(4)` `register?(1)` `write?(1)`

---

## J-type conditions

Assemblers should prefer using these mnemonics, and encode `cond` accordingly.

| num | mnem | condition |
|---|----|---------|
| `0x0` | JMP | `true` |
| `0x1` | JEQ | `Z == 1` |
| `0x2` | JNE | `Z == 0` |
| `0x3` | JLT | `N != V` |
| `0x4` | JGE | `N == V` |
| `0x5` | JLTU | `C == 0` |
| `0x6` | JGEU | `C == 1` |
| `0x7` | JCS | `C == 1` |
| `0x8` | JCC | `C == 0` |
| `0x9` | JN | `N == 1` |
| `0xA` | JP | `N == 0` |
| `0xB` | JVS | `V == 1` |
| `0xC` | JVC | `V == 0` |
| `0xD` | JLS | `C == 0 or Z == 1` |
| `0xE` | reserved | |
| `0xF` | reserved | |

---

## Exact opcode spec table

| Mnem | Flag updates | PC effect | SP effect | Fault cases |
|----|------------|---------|---------|-----------|
| SUB | C,V,Z,N | | | |
| ADD | C,V,Z,N | | | |
| MUL | C,V,Z,N | | | |
| DIV | C,V,Z,N | | | Division by 0 |
| SHL | C,V,Z,N | | | |
| SHR | C,V,Z,N | | | |
| AND | Z,N | | | |
| OR | Z,N | | | |
| NOT | Z,N | | | |
| XOR | Z,N | | | |
| LUI | | | | |
| CMP | Z,N | | | |
| LDR | | | | Out-of-bounds target |
| STR | | | | Out-of-bounds target |
| LDRB | | | | Out-of-bounds target |
| STRB | | | | Out-of-bounds target |
| JXX | | Sets to decoded target if `cond` is true | | Out-of-bounds target |
| CALL | | Pushes to SP, then follows `JXX` logic to jump to target unconditionally | Decrements by 4 | Out-of-bounds target |
| RET | | Pops from SP | Increments by 4 | Out-of-bounds target |
| PUSH | | | On single-register, decrements by 4. On bitmask, decrements by 4 for every set bit | Pushing `SP` |
| POP | | | On single-register, increments by 4. On bitmask, increments by 4 for every set bit | Popping `SP` |
| FLAGS | | | | |

---

## CPU Exceptions

| Name | Number | Description |
|----|------|-----------|
| Invalid instruction | `0x0` | A general error thrown by the decoder if it fails to properly decode an instruction |
| Misaligned PC | `0x1` | Thrown when `PC` is not a multiple of 4 |
| Invalid memory access | `0x2` | Thrown when an instruction attempts to access a memory location that does not exist |
| Stack under/overflow | `0x3/0x4` | Thrown when trying to pop past memory maximum or push past `0x0` |
| reserved | `0x5-0x1F` | |
| Interrupt entry | `0x20-0xFE` | Not a CPU exception, and in fact a hardware interrupt |
| Non-maskable Interrupt | `0xFF` | Thrown by the ICU |

For all exceptions less than `0x20`, the CPU first pushes some details to the stack, then jumps to the address stored in the Interrupt Handler Vector Table:
- `PC = IHVT[irq]`

For Interrupt Entry exceptions, `IE` must be enabled for this to happen. Otherwise the exception is ignored.

The IHVT is a 256-word-long array of function pointers. Its address is `0x00010000`.

---

## External interrupts

On external interrupts (`vec >= 0x20 && (IE == 1 || vec == 0xFF)`):
- Clear `IE`
- Push `FLAGS`
- Push `PC`
- `PC = IHVT[vec]`

---

## Internal interrupts

On internal interrupts (`vec < 0x20`):
- Push `FLAGS`
- Push `PC`
- Push error code if applicable
- Push offending instruction if applicable

---

## Interrupt Controller Unit (ICU)

Currently only supports 32 possible hardware interrupts.

MMIO address: `0x00010400`

- `0x00` - `IRR` (Interrupt Request Register): one bit per IRQ; set by the hardware when a device asserts an interrupt
- `0x04` - `ISR` (In-Service Register): one bit per IRQ; set when the ICU has dispatched an IRQ to the CPU. Cleared by `EOI`
- `0x08` - `IMR` (Interrupt Mask Register): one bit per IRQ; software can mask unwanted interrupts and they get dropped
- `0x0C` - `PRIO[n]` (Priority table): 8-bit priority per IRQ. Higher number = higher priority
- `0x2C` - `VEC[n]` (vector table): 8-bit IHVT vector per IRQ. ICU reads `VEC[irq]` and signals the CPU with that vector (valid range `0x20-0xFE`). If invalid range, ICU uses `DEFAULT` vector
- `0x4C` - `EOI` (write only): write IRQ number to signal end-of-interrupt, clears `ISR[irq]`
- `0x50` - `DEFAULT`: the vector used on invalid vector table entry

---

## System/ABI

### Calling convention

- Argument registers: `R0-R3`
- Return register: `R0`
- Callee-saved: `R4-R11`
- Caller-saved/temporaries: `R12-R14`

### Stack conventions and frame layout

- Stack grows down
- `SP` points to next free byte
- Stack alignment: 8-byte alignment at call boundaries (`SP % 8 == 0`)
- All stack allocations must preserve this

Caller responsibilities:
- place additional args (beyond four) on stack, pushed right-to-left
- align stack before `CALL`
- caller cleans up stack after return (`cdecl` style)

Function prologue:
- `PUSH R4..Rn` for callee-saved used
- `SUB SP, SP, <localsize>` (`localsize` rounded to 8)

Epilogue:
- restore callee-saved
- `RET` (which pops `PC`)

---

## Exception/Interrupt stack frames

Internal exception entry (`vec < 0x20`):
- push `FLAGS`
- push `PC`
- error code
- offending instruction
- `PC = IHVT[vec]`

External interrupt:
- if `IE == 1` or `vec == 0xFF`:
  - clear `IE`
  - push `FLAGS`
  - push `PC`
  - `PC = IHVT[vec]`

---

## Peripheral Configuration Unit (PCU)

Dedicated hardware block at a permanently hardcoded slice of memory: `0x00010600` to `0x0001FFFF`.

Consists of 16 devices. Each device is 16 bytes.

| Offset | Name | Description |
|------|----|-----------|
| `0x00` | Device ID register (RO) | Hardcoded 32-bit number unique to the device type. Split into Vendor(12), Class(8), Device(8), Revision(4) |
| `0x04` | Base Address register (RW) | Contains the address for the memory mapping. BIOS sets this to `0` on system reset |
| `0x08` | Component Size register (RO) | Hardcoded value representing how many bytes of address space the device requires |
| `0x0C` | Reserved | For potential future updates |

### PCU Device Class IDs

| ID | Meaning |
|--|-------|
| `0x00` | Reserved |
| `0x01` | Mass storage |
| `0x02` | Network/comms |
| `0x03` | Display/graphics |
| `0x04` | Input |
| `0x05` | System infrastructure (ICU, timers, etc) |

---

## Memory Mapping

| Start addr | End addr | Size | Name | Desc |
|----------|--------|----|----|-----------|
| `0x00000000` | `0x00000FFF` | 4KB | Zero page | Strictly unmapped. Any read/write attempts here trigger bus fault |
| `0x00001000` | `0x0000FFFF` | 60KB | Boot ROM | Contains the boot code, and the hardware reset vector `0x00001000` |
| `0x00010000` | `0x000103FF` | 1KB | IHVT | |
| `0x00010400` | `0x000104FF` | 256B | ICU | Fixed control registers for interrupt management |
| `0x00010500` | `0x000105FF` | 256B | Timers | Fixed hardware tick counters |
| `0x00010600` | `0x0001FFFF` | 62.5KB | PCU | |
| `0x00020000` | `0xFFFFFFFF` | ~4.3GB | System RAM | |

---

## Devices

### GPU

#### Device ID:
- Vendor `0x123`
- Class `0x03`
- Device `0x01`
- Revision `0x0`

#### Registers:
|Offset|Name|Perms|Desc|
|------|----|-----|----|
|`0x00`|Framebuffer Base Address|RW|Tells the hardware where in RAM it should look to draw pixels. On reset, this is `0` (display disabled). The boot ROM will configure this to `0x00020000`|
|`0x04`|Display Status (VBLANK)|RO|Bit 0: High if screen is currently in a Vertical Blanking interval, low if it is drawing.|
|`0x08`|Interrupt Configuration|RW|Bit 0: Enable VBLANK interrupt. If high, the display hardware will automatically trigger an ICU interrupt every time a frame finishes drawing.|
|`0x0C`|Video Mode|RW|Selects the active display mode pipeline.|

#### Video modes:
* `0x0`: Core framebuffer disabled
* `0x1`: 320x240 RGB565 Linear Framebuffer
* `0x2..0xF`: Reserved for future hardware graphics accelerators.

---
