# Orion design spec

## Registers

- `R0..R14` general-purpose
- `R15` is `SP` (Stack Pointer)
- Private `PC` and flags registers

#### Fully Banked Register File:  
The CPU maintains two physically separate 16-register files: the User Bank (`USR`) and the Kernel Bank (`KSR`).
- When `FLAGS.PRV == 0`, the CPU executes using the `USR` bank.
- When `FLAGS.PRV == 1`, the CPU instantly switches to the `KSR` bank.
- `SP` is fully banked, ensuring independent stack pointers for User space (`USP`) and Kernel space (`KSP`).

`SP` and `PC` are both stored as byte addresses, but must be word-aligned.
`PC` always points to the current instruction being executed.

---

## Flags register

The Flags register is a private register containing all CPU system states.

| Bit | Flag name | Desc |
|---|---------|----|
| 0 | Interrupts Enabled (`IE`) | When false, external interrupts are ignored. |
| 1 | Privilege Level (`PRV`) | `0` = User Mode. `1` = Supervisor/Kernel Mode. |
| 2 | Virtual Memory (`VM`) | `0` = Paging disabled (Physical 1:1). `1` = Paging active. |
| 3 | Translation Mode (`TM`) | Set by hardware during a TLB miss. Bypasses the MMU for all data access paths. |
| 4..31 | Reserved | Must be 0. |

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

- `J-type`: `JMP`, `CALL` (Unconditional jumps with massive range)
- `B-type`: `JEQ`, `JLT`, `JGE` (Conditional branches via register comparison)
- `A-type`: `ADD`, `SUB`, `SHL`, etc. (Pure maths, no flag updates)
- `M-type`: `LDR`, `STR`
- `S-type`: `FLAGS`

---

## Instruction formats

`J-type` (Unconditional Jumps):
- `opcode(6)` `imm(26)`

`B-type` (Conditional Branches):
- `opcode(6)` `cond(4)` `rn(4)` `rd(4)` `absolute?(1)` `reserved(1)` `imm(12)`

`A-type`:
- `opcode(6)` `rn(4)` `rd(4)` `(rm(4) | imm(16))` `register?(1)` `reserved(1)`

`M-type`:
- `opcode(6)` `rn(4)` `rd(4)` `(rm(4) | imm(16))` `register?(1)` `reserved(1)`

(note, mathematical operations like `ADD`, `SUB`, `STR`, etc. will sign-extend their immediates, whilst bitwise operations like `AND`, `OR`, `XOR`, will zero-extend their immediates.)

`S-type`:
- specified per instruction

`J-type` decoding:
- `PC = PC + sign_extend(imm26)` (If absolute opcode used, `PC = imm26`)

`B-type` decoding:
- If `cond` evaluates to true comparing `R[rn]` and `R[rd]`:
  - If `absolute?` is set: `PC = imm`
  - Otherwise: `PC += sign_extend(imm)`

---

## `FLAGS` encoding

- `opcode(6)` `(rd(4) | (rm(4) | imm(16)))` `reserved(4)` `register?(1)` `write?(1)`

---

## B-type conditions

| num | mnem | condition |
|---|----|---------|
| `0x0` | reserved | (Unconditional jumps moved to J-Type) |
| `0x1` | JEQ | `R[rn] == R[rd]` |
| `0x2` | JNE | `R[rn] != R[rd]` |
| `0x3` | JLT | `R[rn] < R[rd]` (Signed) |
| `0x4` | JGE | `R[rn] >= R[rd]` (Signed) |
| `0x5` | JLTU | `R[rn] < R[rd]` (Unsigned) |
| `0x6` | JGEU | `R[rn] >= R[rd]` (Unsigned) |
| `0x7..0xF` | reserved | |

---

## Exact opcode spec table

| Number | Mnemonic | Type | Description | Fault cases |
|--------|----------|------|-------------|-------------|
| `0x00` | NOP | x | Does nothing for a cycle |  |
| `0x01` | SUB | A | Subtraction |  |
| `0x02` | ADD | A | Addition |  |
| `0x03` | MUL | A | Multiplication |  |
| `0x04` | DIV | A | Signed division | Divisor == 0 |
| `0x05` | DIVU | A | Unsigned division | Divisor == 0 |
| `0x06` | SHL | A | Logical left shift |  |
| `0x07` | SHR | A | Signed logical right shift |  |
| `0x08` | SHRU | A | Unsigned logical right shift |  |
| `0x09` | AND | A | Bitwise and |  |
| `0x0A` | OR | A | Bitwise or |  |
| `0x0B` | XOR | A | Bitwise xor |  |
| `0x0C` | LUI | A | Loads `rm`/`imm` into top 16 bits of `rd` (ignores `rn`) | |
| `0x0D` | LDR | M | Load a word | Out-of-bounds, Priv Violation (MMIO) |
| `0x0E` | STR | M | Store a word | Out-of-bounds, Priv Violation (MMIO) |
| `0x0F` | LDRB | M | Load a byte | Out-of-bounds, Priv Violation (MMIO) |
| `0x10` | STRB | M | Store a byte | Out-of-bounds, Priv Violation (MMIO) |
| `0x11` | JMP | J | Unconditional relative jump | Out-of-bounds |
| `0x12` | JMPA | J | Unconditional absolute jump | Out-of-bounds |
| `0x13` | JXX | B | Conditional branch based on register comparison | Out-of-bounds |
| `0x14` | CALL | J | Pushes PC and jumps relative unconditionally | Out-of-bounds |
| `0x15` | CALLA | J | Pushes PC and jumps absolute unconditionally | Out-of-bounds |
| `0x16` | RET | x | Pops PC (Ignores immediate fields) | Out-of-bounds |
| `0x17` | FADD | A | Floating-point Addition (IEEE 754) | Float Overflow/Underflow |
| `0x18` | FSUB | A | Floating-point Subtraction | Float Overflow/Underflow |
| `0x19` | FMUL | A | Floating-point Multiplication | Float Overflow/Underflow |
| `0x1A` | FDIV | A | Floating-point Division | Divisor == 0.0, Invalid Op |
| `0x1B` | FCMP | A | Float Compare. `R[rd] = -1` if `rn < rm`; `0` if equal; `1` if `rn > rm` | |
| `0x1C` | ITOF | A | Convert 32-bit Integer in `R[rm]` to Float in `R[rd]` | |
| `0x1D` | FTOI | A | Convert 32-bit Float in `R[rm]` to Integer in `R[rd]` (Truncate) | Float out of Integer bounds |
| `0x1E` | XCHG | M | Atomically reads a word from memory address `rn + (rm/imm)` into a temporary buffer, writes `rd` to that address, and stores the temporary buffer back into `rd` | Out-of-bounds, Priv violation, Misaligned data |
| `0x1F` | reserved | | |
| `0x20` | RPC | S | Reads `pc` to `rd` | |
| `0x21` | FLAGS | S | Reads `flags` to `rd` or writes `rm`/`imm` to `flags` | Priv Violation (Write when PRV=0) |
| `0x22` | HALT | x | Pauses the CPU until an interrupt fires | Priv Violation (If PRV=0) |
| `0x23` | SYSCALL | S | Triggers a software interrupt | |
| `0x24` | IRET | x | Pops `PC`, then pops `FLAGS`. Atomically restores privilege/state. | Priv violation (if PRV=0) |

---

## System power-on and reset state

When the CPU receives a hardware reset signal or powers on, it initialises its internal state machines to a strict, predictable baseline before executing the first clock cycle.

### Register reset values

`PC` = `0x00001000` (Points to beginning of Boot ROM)  
`FLAGS` = `0x00000002` (Privilege level set to `Supervisor Mode`,   Paging `Disabled`, Interrupts `Disabled`)
`USR`/`KSR` `R0-15` = Undefined (it is recommended that emulators zero out registers anyways, but it cannot be relied upon)  

### Flag state explanation

|Flag|Reset value|Meaning|
|----|-----------|-------|
|`IE` (Bit 0) | `0` | Interrupts disabled. The bootloader must configure the IHVT and the ICU before enabling |
|`PRV` (Bit 1) | `1` | Kernel mode active. Allows full access to system architecture mapping |
|`VM` (Bit 2) | `0` | Virtual memory active paging disabled. Address lines mirror physical RAM 1:1 |
|`TM` (Bit 3) | `0` | Translation mode disabled. Normal memory bus access rules apply | 

### Subsystem Initialisation State

The only subsystem that enforces initialisation state is the Peripheral Control Unit. It ensures that all device BARs are set to `0` (aka unmapped).

---

## CPU Exceptions

| Name | Number | Description |
|----|------|-----------|
| Invalid instruction | `0x0` | A general error thrown by the decoder if it fails to properly decode an instruction |
| Misaligned PC | `0x1` | Thrown when `PC` is not a multiple of 4 |
| Invalid memory access | `0x2` | Thrown when an instruction attempts to access a memory location that does not exist |
| Stack under/overflow | `0x3/0x4` | Thrown when trying to pop past memory maximum or push past `0x0` |
| Instruction TLB Miss | `0x5` | Fetching the next PC instruction missed the TLB |
| Data TLB Miss | `0x6` | An LDR/STR instruction missed the TLB |
| Page Fault | `0x7` | TLB entry exists, but access was violated (e.g., User writing to read-only page) |
| System Call | `0x8` | Thrown intentionally by the `SYSCALL` instruction |
| Arithmetic Fault | `0x9` | Thrown by the ALU or FPU (e.g., Division by Zero, Float Overflow, Invalid Float Op) |
| Misaligned Data | `0xA` | Thrown when `LDR`, `STR`, or `XCHG` attempt to access a word at a memory address that is not a multiple of 4|
| reserved | `0xB-0x1F` | |
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

Because the CPU automatically swaps to the `KSR` register bank on an exception, no user registers need to be pushed. All stack operations below occur on the Kernel Stack (`KSP`).

Internal exception entry (`vec < 0x20`):
- Hardware saves offending virtual address to `MTU_FAULT_ADDR` (if memory-related fault)
- push `FLAGS`
- push `PC`
- push error code if applicable
- push offending instruction if applicable
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

| Start addr | End addr | Size | Name | Desc |
|----------|--------|----|----|-----------|
| `0x00000000` | `0x00000FFF` | 4KB | Zero page | Strictly unmapped. Any read/write attempts here trigger bus fault |
| `0x00001000` | `0x0000FFFF` | 60KB | Boot ROM | Contains the boot code, and the hardware reset vector `0x00001000` |
| `0x00010000` | `0x000103FF` | 1KB | IHVT | |
| `0x00010400` | `0x000104FF` | 256B | ICU | Fixed control registers for interrupt management |
| `0x00010500` | `0x000105FF` | 256B | Timers | Fixed hardware tick counters |
| `0x00010600` | `0x000107FF` | 512B | MTU | Memory Translation Unit & Suspended USR Register Portal |
| `0x00010800` | `0x0001FFFF` | ~60KB| PCU | Peripheral Configuration Unit |
| `0x00020000` | `0xFFFFFFFF` | ~4.3GB | System RAM | |

---

## Programmable Interval Timer (PIT)

### Timer Control Registers (`0x00010500`)

|Offset|Name|Perms|Description|
|------|----|-----|-----------|
|`0x00`|`TIMER_CTRL`|RW|Bit 0: enable timer. bit 1: auto-reload after firing. bit 2: interrupt enable|
|`0x04`|`TIMER_INTERVAL`|RW|The target value in nanoseconds before the timer expires|
|`0x08`|`TIMER_CURRENT`|RO|The current tick count of the timer|
|`0x0C`|`TIMER_ACK`|WO|Writing any value clears the pending timer interrupt in the ICU|

---

## Memory Translation Unit (MTU)

The MTU handles all Virtual-to-Physical memory mapping via a 32-slot Software-Refilled Translation Lookaside Buffer (TLB). It is fully memory-mapped, requiring no special opcodes to manage.

### Virtual Address Format (32-bit, 4KB Pages)
- Bits 31..12: Virtual Page Number (`VPN`, 20 bits)
- Bits 11..0: Byte Offset (12 bits)

### MTU Control Registers (`0x00010600 - 0x0001063F`)
| Offset | Name | Perms | Description |
|--------|------|-------|-------------|
| `0x00` | `MTU_FAULT_ADDR` | RO | Holds the exact 32-bit virtual address that caused the last miss/fault. |
| `0x04` | `MTU_CONFIG`     | RW | Bits 0..4: Indexes the target hardware TLB entry (0..31) for configuration slots. |
| `0x08` | `TLB_HI_WIN`     | RW | Writes the high-word (VPN, ASID, Valid bit) to the active TLB index. |
| `0x0C` | `TLB_LO_WIN`     | RW | Writes the low-word (PFN, Permissions) to the active TLB index. |
| `0x10` | `MTU_CURR_ASID`  | RW | The ASID (0-1023) of the currently executing thread. |

### Suspended USR Register File Portal (`0x00010640 - 0x0001067F`)
When `PRV == 1`, reading or writing to these offsets directly manipulates the frozen user-mode register file, allowing zero-cost context switching.

| Offset | Name | Perms | Description |
|--------|------|-------|-------------|
| `0x40` | `USR_R0`  | RW | Suspended User `R0` (Argument / Return value slot) |
| `0x44` | `USR_R1`  | RW | Suspended User `R1` |
| ...    | ...       | ...| ... |
| `0x78` | `USR_R14` | RW | Suspended User `R14` |
| `0x7C` | `USR_SP`  | RW | Suspended User `R15` (`SP`). |

### TLB Window Bit Layouts
When writing to `TLB_HI_WIN` or `TLB_LO_WIN`, the 32-bit words must be formatted as follows:

TLB_HI_WIN (Virtual Word):
- `Bits 31..12`: Virtual Page Number (`VPN`)
- `Bits 11..1`: Address Space ID (`ASID`) - Used to distinguish process spaces without flushing the TLB.
- `Bit 0`: Valid (`V`) - `1` = Entry active, `0` = Entry triggers Page Fault.

TLB_LO_WIN (Physical Word):
- `Bits 31..12`: Physical Frame Number (`PFN`)
- `Bits 11..4`: Reserved (Must be 0)
- `Bit 3`: Writable (`W`) - `1` = Read/Write, `0` = Read-Only.
- `Bit 2`: User (`U`) - `1` = Accessible in User Mode, `0` = Kernel Only.
- `Bit 1`: Cacheable (`C`) - `1` = Standard RAM cache, `0` = Uncached (MMIO bypass).
- `Bit 0`: Wired (`WI`) - `1` = Hardware cannot evict this entry naturally.

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

# scratchpad

