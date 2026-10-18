# Orion design spec

## Registers

- `R0..R14` general-purpose registers
- `R15` is `SP` (Stack Pointer)
- Private `PC` and `FLAGS` registers

#### Fully Banked Register File

The CPU maintains two physically separate 16-register files: the User Bank (`USR`) and the Kernel Bank (`KSR`).

- When `FLAGS.PRV == 0`, the CPU executes using the `USR` bank.
- When `FLAGS.PRV == 1`, the CPU executes using the `KSR` bank.
- Changing `PRV` selects the corresponding bank through hardware multiplexing.
- `SP` is fully banked, ensuring independent user and kernel stack pointers (`USP` and `KSP`).

`SP` and `PC` are byte addresses and must be word-aligned when used for stack words or instruction fetches. `PC` identifies the instruction currently executing. General-purpose registers are 32 bits wide.

---

## Flags register

`FLAGS` is a private 32-bit register containing CPU system state.

| Bit | Flag name | Description |
|---|---|---|
| 0 | Interrupts Enabled (`IE`) | Enables maskable external interrupts when set. |
| 1 | Privilege Level (`PRV`) | `0` = User Mode. `1` = Kernel/Supervisor Mode. Also selects the active register bank. |
| 2 | Virtual Memory (`VM`) | `0` = direct physical addressing. `1` = virtual-address translation enabled. |
| 3 | Translation Mode (`TM`) | Hardware-set translation bypass used by the TLB-miss handler. While set, instruction fetches and data accesses use physical addresses directly. |
| 4..31 | Reserved | Read as zero. Writes are ignored. |

`TM` takes precedence over `VM`. If `TM == 1`, the TLB is bypassed even when `VM == 1`.

The `FLAGS` instruction can write `IE`, `PRV`, and `VM` in kernel mode. Software cannot directly change `TM`; it is controlled by hardware during TLB-miss entry and restored by `IRET`. Writing `PRV` changes the active register bank immediately. Reserved flag bits read as zero. The OS must not execute untrusted user code while `VM=0`, because direct physical addressing does not provide page-based isolation.

---

## Instruction properties

- Fixed 32-bit instructions, little-endian.
- General-purpose registers are 32-bit.
- Memory is byte-addressable.
- Instruction fetches and word accesses require 4-byte alignment.
- Instructions are always 4 bytes long.
- All relative control-transfer offsets are signed counts of 32-bit instruction words, not bytes.
- Relative control transfers use `PC + 4` as their base.
- Stack grows downwards; `SP` points to the next free byte.
- Integer arithmetic wraps modulo $2^{32}$, except for defined faulting operations.
- Arithmetic instructions do not update `FLAGS`.
- CPU memory operations support bytes and 32-bit words; MMIO devices define any register-specific access-width restrictions.

---

## Encoding types

- `J-type`: `JMP`, `CALL` (relative transfers with a 26-bit word displacement)
- `I-type`: `JMPR`, `CALLR` (register-indirect transfers)
- `B-type`: `JXX` (conditional branch)
- `A-type`: arithmetic, bitwise, shift, conversion, and floating-point operations
- `M-type`: loads, stores, and atomic exchange
- `S-type`: `FLAGS`, `RPC`
- `X-type`: instructions without explicit operands

All reserved instruction bits must be zero. An unsupported opcode or invalid encoding raises an Invalid Instruction exception.

---

## Instruction formats

### `J-type` (`JMP`, `CALL`)

- `opcode(6)` `displacement(26)`

The displacement is a signed count of instruction words:

`target = PC + 4 + sign_extend(displacement) * 4`

`JMP` assigns `target` to `PC`. `CALL` pushes `PC + 4` to the active stack before assigning `target` to `PC`.

The 26-bit signed word displacement reaches from `-128 MiB` to `+128 MiB - 4 bytes`, relative to `PC + 4`.

### `I-type` (`JMPR`, `CALLR`)

- `opcode(6)` `rn(4)` `reserved(22)`

`JMPR` assigns `R[rn]` to `PC`. `CALLR` pushes `PC + 4` and then assigns `R[rn]` to `PC`. The target must be word-aligned. The former `JMPA` and `CALLA` opcode numbers are reused for `JMPR` and `CALLR`; no additional opcode numbers are introduced.

### `B-type` (`JXX`)

- `opcode(6)` `cond(3)` `rn(4)` `operand(5)` `displacement(13)` `register?(1)`

`rn` selects the left comparison operand. If `register?` is clear, `operand` is an unsigned 5-bit immediate (`0..31`). If `register?` is set, `operand[3..0]` selects the right-hand register and `operand[4]` must be zero.

If the condition is true, the target is:

`target = PC + 4 + sign_extend(displacement) * 4`

Otherwise `PC` advances to `PC + 4`. The 13-bit signed displacement reaches from `-16 KiB` to `+16 KiB - 4 bytes`, relative to `PC + 4`.

### `A-type` and `M-type`

- `opcode(6)` `rn(4)` `rd(4)` `operand(16)` `register?(1)` `reserved(1)`

The low reserved bit must be zero. If `register?` is set, `operand[15..4]` must be zero and `operand[3..0]` selects `rm`. Otherwise, `operand` is a 16-bit immediate.

For ordinary A-type operations, `rn` is the left operand and `rd` is the destination unless an instruction specifies otherwise. For M-type operations, the effective address is:

- Register form: `R[rn] + R[rm]`
- Immediate form: `R[rn] + sign_extend(imm16)`

Effective addresses are calculated modulo \(2^{32}\). Memory offsets are byte offsets, unlike control-transfer displacements, which are measured in instruction words.

### `S-type` (`FLAGS`, `RPC`)

`FLAGS` encoding:

- `opcode(6)` `reg(4)` `payload(16)` `reserved(4)` `register?(1)` `write?(1)`

For a read (`write? == 0`), `reg` is the destination register; `payload`, `register?`, and reserved bits must be zero. For a register write (`write? == 1`, `register? == 1`), `reg` selects the source register and `payload` plus reserved bits must be zero. For an immediate write (`write? == 1`, `register? == 0`), `reg` must be zero and `payload` is the zero-extended 16-bit value written to `FLAGS`.

Writing `FLAGS` is privileged. `TM` cannot be changed by a software write; reserved flag bits are ignored. A user-mode write raises a Privilege Violation exception.

`RPC` uses `opcode(6)` `rd(4)` `reserved(22)`. It reads the address of the current `RPC` instruction into `R[rd]`; all reserved bits must be zero.

### `X-type`

`NOP`, `RET`, `FENCE`, `HALT`, `SYSCALL`, and `IRET` use the opcode with all remaining bits zero. `JMPR` and `CALLR` use their `I-type` encoding.

---

## B-type conditions

| Value | Mnemonic | Condition |
|---|---|---|
| `0x0` | Reserved | Invalid instruction. |
| `0x1` | `JEQ` | `R[rn] == rhs` |
| `0x2` | `JNE` | `R[rn] != rhs` |
| `0x3` | `JLT` | Signed `R[rn] < rhs` |
| `0x4` | `JGE` | Signed `R[rn] >= rhs` |
| `0x5` | `JLTU` | Unsigned `R[rn] < rhs` |
| `0x6` | `JGEU` | Unsigned `R[rn] >= rhs` |
| `0x7` | Reserved | Invalid instruction. |

For immediate comparisons, `rhs` is the zero-extended value `0..31`. For register comparisons, `rhs` is the full 32-bit value of the selected register.

---

## Exact opcode spec table

| Number | Mnemonic | Type | Description | Fault cases |
|---|---|---|---|---|
| `0x00` | `NOP` | X | No state change other than advancing to the next instruction. | Invalid encoding |
| `0x01` | `SUB` | A | Subtraction; wraps modulo $2^{32}$. | |
| `0x02` | `ADD` | A | Addition; wraps modulo $2^{32}$. | |
| `0x03` | `MUL` | A | Low 32 bits of multiplication result. | |
| `0x04` | `DIV` | A | Signed division, truncating toward zero. | Zero divisor; `INT32_MIN / -1` |
| `0x05` | `DIVU` | A | Unsigned division. | Zero divisor |
| `0x06` | `SHL` | A | Logical left shift. Shift count uses low 5 bits. | |
| `0x07` | `SHR` | A | Logical right shift; high bits are filled with zero. | |
| `0x08` | `ASHR` | A | Arithmetic right shift; high bits copy the sign bit. | |
| `0x09` | `AND` | A | Bitwise AND. | |
| `0x0A` | `OR` | A | Bitwise OR. | |
| `0x0B` | `XOR` | A | Bitwise XOR. | |
| `0x0C` | `LUI` | A | Writes the low 16 bits of `rm`/`imm` into the high 16 bits of `rd`; low 16 bits are cleared. `rn` is ignored. | |
| `0x0D` | `LDR` | M | Load 32-bit word. | Invalid address; page fault; misaligned data |
| `0x0E` | `STR` | M | Store 32-bit word. | Invalid address; page fault; misaligned data |
| `0x0F` | `LDRB` | M | Load byte and zero-extend to 32 bits. | Invalid address; page fault |
| `0x10` | `STRB` | M | Store low byte of `rd`. | Invalid address; page fault |
| `0x11` | `JMP` | J | Relative unconditional jump. | Misaligned target on fetch |
| `0x12` | `JMPR` | I | Register-indirect jump. | Misaligned target on fetch |
| `0x13` | `JXX` | B | Conditional comparison and relative branch. | Misaligned target on fetch |
| `0x14` | `CALL` | J | Pushes `PC + 4` and performs relative call. | Stack overflow; memory fault |
| `0x15` | `CALLR` | I | Pushes `PC + 4` and performs register-indirect call. | Stack overflow; memory fault |
| `0x16` | `RET` | X | Pops a return PC from the active stack. | Stack underflow; memory fault |
| `0x17` | `FADD` | A | IEEE 754 binary32 addition. Register form only. | Invalid encoding |
| `0x18` | `FSUB` | A | IEEE 754 binary32 subtraction. Register form only. | Invalid encoding |
| `0x19` | `FMUL` | A | IEEE 754 binary32 multiplication. Register form only. | Invalid encoding |
| `0x1A` | `FDIV` | A | IEEE 754 binary32 division. Register form only. | Invalid encoding |
| `0x1B` | `FCMP` | A | Compares `rn` with `rm`; writes `-1`, `0`, `1`, or `2` to `rd`. Register form only. | Invalid encoding |
| `0x1C` | `ITOF` | A | Convert signed 32-bit integer in `rm` to binary32 in `rd`. Register form only. | Invalid encoding |
| `0x1D` | `FTOI` | A | Convert binary32 in `rm` to signed 32-bit integer in `rd`, truncating toward zero. | NaN, infinity, or out-of-range input; invalid encoding |
| `0x1E` | `XCHG` | M | Atomically exchanges a word in ordinary RAM with `rd`; old memory value is written to `rd`. | Invalid address; permission fault; misaligned data; MMIO access |
| `0x1F` | `FENCE` | X | Full CPU memory-ordering barrier. Does not wait for asynchronous device completion. | |
| `0x20` | `RPC` | S | Reads the current instruction address into a register. | Invalid encoding |
| `0x21` | `FLAGS` | S | Reads `FLAGS` or writes it in kernel mode. | Privilege violation; invalid encoding |
| `0x22` | `HALT` | X | Stops instruction execution until a deliverable interrupt. | Privilege violation; invalid encoding |
| `0x23` | `SYSCALL` | X | Raises synchronous exception vector `0x08`. Return PC is `PC + 4`. | |
| `0x24` | `IRET` | X | Restores the standard exception frame and returns. | Privilege violation; memory fault |
| `0x25..0x3F` | Reserved | -- | Invalid opcode. | Invalid instruction |

### Immediate interpretation

Immediate extension is defined by opcode; there is no signedness bit.

| Instruction | Immediate interpretation |
|---|---|
| `ADD`, `SUB`, `MUL`, `DIV` | Sign-extend 16-bit immediate. |
| `DIVU` | Zero-extend 16-bit immediate. |
| `AND`, `OR`, `XOR` | Zero-extend 16-bit immediate. |
| `LUI` | Use low 16 bits as unsigned value and shift left 16. |
| `LDR`, `STR`, `LDRB`, `STRB`, `XCHG` | Sign-extend 16-bit byte offset. |
| `SHL`, `SHR`, `ASHR` | Use the low five bits of the selected shift-count operand. |
| `FADD`, `FSUB`, `FMUL`, `FDIV`, `FCMP`, `ITOF`, `FTOI` | Register form only; immediate encoding is invalid. |

Integer `ADD`, `SUB`, and `MUL` never fault due to overflow. `DIV` and `DIVU` fault on a zero divisor. Signed `DIV` also faults on `INT32_MIN / -1`. A faulting instruction must not commit partial architectural results; for example, a faulting division leaves `rd` unchanged.

### Floating-point semantics

CPU floating-point values use IEEE 754 binary32 with round-to-nearest, ties-to-even. Subnormal numbers, signed zero, infinities, and NaNs are supported. Floating-point overflow, underflow, and division by zero do not trap; results follow IEEE 754 arithmetic. Invalid operations produce the canonical positive quiet NaN bit pattern `0x7FC00000` for deterministic results.

`FCMP` writes `-1` if `R[rn] < R[rm]`, `0` if equal, `1` if greater, and `2` if unordered due to NaN. `ITOF` converts a signed 32-bit integer to binary32. `FTOI` truncates toward zero and raises an Arithmetic Fault if the input is NaN or infinite, or if the truncated result is outside the signed 32-bit integer range. The destination is unchanged on fault.

### Memory ordering and atomic exchange

`XCHG` uses the M-type effective-address calculation, requires a word-aligned address in ordinary RAM, and requires both read and write permission. It is indivisible with respect to all physical-memory agents, including the CPU, DMA devices, and Lyra. `XCHG` to MMIO raises an Invalid Memory Access exception. Address and permission checks complete before the exchange commits.

`FENCE` is a full CPU memory-ordering barrier. Earlier CPU memory accesses complete, and earlier CPU writes become globally visible to other physical-memory agents, before later CPU memory accesses are allowed to take effect. Any required cache writebacks or ordering operations complete before `FENCE` retires. `FENCE` does not wait for a device to finish processing previously submitted commands; device-specific fence mechanisms provide that guarantee.

---

## System power-on and reset state

The CPU reset state is deliberately minimal. Before the first instruction fetch:

- `PC = 0x00001000` (beginning of Boot ROM).
- `FLAGS = 0x00000002` (`PRV=1`, `IE=0`, `VM=0`, `TM=0`).

All other state that can be left unspecified is unspecified at reset, including the general-purpose registers in both banks, both stack pointers, RAM contents, TLB entries, the current ASID, interrupt-controller state, timer state, peripheral state, and peripheral BAR values. Firmware must initialize every item it relies on.

Fixed system-address decoding takes precedence over peripheral BAR mappings, including at reset. Firmware must initialize `KSR.SP` before code that may take an exception; configure the IHVT and ICU; mask and acknowledge interrupt sources; initialize the MTU and invalidate every TLB entry before enabling `VM`; initialize RAM, stacks, and page tables; configure or unmap BARs; and only then enable the facilities it needs. Implementations must ensure enough private CPU state is initialized to fetch and execute from the reset vector.

---

## CPU Exceptions

| Name | Number | Description |
|---|---|---|
| Invalid Instruction | `0x00` | Invalid opcode, reserved bits, or invalid instruction encoding. |
| Misaligned PC | `0x01` | Instruction fetch at an address not divisible by four. |
| Invalid Memory Access | `0x02` | Unmapped physical address, invalid bus access, unsupported MMIO access, or a memory access that cannot be completed. |
| Stack Underflow | `0x03` | An implicit stack pop would wrap the 32-bit stack pointer. |
| Stack Overflow | `0x04` | An implicit stack push would underflow the 32-bit stack pointer. |
| Instruction TLB Miss | `0x05` | Instruction fetch misses the TLB while translation is enabled. |
| Data TLB Miss | `0x06` | Data access misses the TLB while translation is enabled. |
| Page Fault | `0x07` | A valid TLB entry exists but its permissions deny the access. |
| System Call | `0x08` | Intentional exception raised by `SYSCALL`. |
| Arithmetic Fault | `0x09` | Integer division fault or invalid/out-of-range `FTOI`. |
| Misaligned Data | `0x0A` | A word load, store, or `XCHG` uses an address not divisible by four. |
| Privilege Violation | `0x0B` | User mode executes a privileged operation. |
| Reserved | `0x0C..0x1F` | Reserved for future synchronous exceptions. |
| External Interrupt | `0x20..0xFE` | Hardware interrupt vector assigned by the ICU. |
| Non-maskable Interrupt (NMI) | `0xFF` | Non-maskable interrupt. Ignores `IE` and may recursively nest. |

For synchronous exceptions, hardware saves `PC + 4` as the return PC. The instruction must not commit partial results before raising an exception. If a handler wants to retry the faulting instruction, it subtracts four from the saved return PC before executing `IRET`.

The Interrupt Handler Vector Table (IHVT) contains 256 32-bit handler addresses at physical address `0x00010000`. The vector entry for exception or interrupt `v` is at `0x00010000 + 4*v`. The table itself is always accessed physically. A handler address is interpreted as physical when `VM=0` or `TM=1`; otherwise it is interpreted as a virtual address.

---

## Exception/Interrupt stack frames

Every exception and interrupt creates a standard two-word frame on the Kernel Stack (`KSP`). Hardware first captures the original `FLAGS` and return PC, disables live maskable interrupts (`IE=0`), sets `PRV=1` (switching to the kernel bank), and, for a TLB miss, sets `TM=1`. It then pushes the captured `FLAGS`, followed by the captured return PC.

Immediately after the standard frame is pushed:

- `[KSP + 0]` is the saved return PC.
- `[KSP + 4]` is the saved `FLAGS`.

Some exception vectors may append additional 32-bit information after the standard frame is pushed. Because the stack grows down, each extension word is at a lower address than the saved PC, and `SP` points to the last extension. The handler for that vector must remove any extension and restore `SP` to the standard-frame position before executing `IRET`. The base architecture pushes the fetched instruction word for Invalid Instruction and Privilege Violation exceptions. Memory fault information is available through the MTU fault registers.

### Return-PC rules

- Synchronous exceptions save the faulting instruction's address plus four. A handler subtracts four from the saved PC to retry it.
- `SYSCALL` saves `PC + 4`, returning to the instruction following the system call.
- External interrupts and NMI are accepted at instruction boundaries and save the next architectural PC, so the completed instruction is not repeated.
- A handler may modify the saved PC to skip an instruction or alter the return path.

### `IRET`

`IRET` is privileged and must execute in kernel mode. The kernel must first remove any exception-specific extension fields. Hardware reads the saved PC and `FLAGS` without partially consuming the frame if a read fails. Once both reads succeed, it advances the kernel stack pointer by eight, restores `PC`, and restores `FLAGS` last. Restoring `PRV` may switch back to the user bank; this happens only after the kernel stack frame has been consumed.

If hardware cannot construct the standard frame, read the vector entry, or transfer to its handler because exception entry itself faults, the CPU enters a terminal double-fault state. It stops executing instructions until reset. Exceptions may nest in kernel mode if entry remains possible. NMI has no hardware in-progress suppression and may recursively nest; exhausting the stack or otherwise failing exception entry leads to double-fault state.

---

## External interrupts

Maskable external interrupts are accepted only when `IE == 1` and the ICU reports an eligible request. NMI vector `0xFF` is accepted regardless of `IE`.

On interrupt entry:

1. Capture original `FLAGS` and the next architectural PC.
2. Clear live `IE`.
3. Set `PRV=1`, selecting the kernel register bank.
4. Push the standard exception frame on `KSP` (and any defined vector-specific extension).
5. Load `PC` from `IHVT[vec]`.

Handlers may re-enable `IE` after saving any kernel state that nested handlers must preserve. NMI may recursively nest. External interrupts are sampled at instruction boundaries, not partway through an instruction.

---

## Interrupt Controller Unit (ICU)

The ICU supports 32 hardware interrupt request lines.

MMIO address: `0x00010400`. The ICU is supervisor-only; a user-mode access raises a Privilege Violation exception.

| Offset | Name | Access | Description |
|---|---|---|---|
| `0x00` | `IRR` | RO | One pending bit per IRQ. Masking does not discard a pending request. |
| `0x04` | `ISR` | RO | One bit per IRQ currently in service. Cleared by `EOI`. |
| `0x08` | `IMR` | RW | One mask bit per IRQ. Masked requests remain pending. |
| `0x0C..0x2B` | `PRIO[n]` | RW byte | Eight-bit priority for each IRQ. Higher numbers mean higher priority. |
| `0x2C..0x4B` | `VEC[n]` | RW byte | Eight-bit IHVT vector for each IRQ; valid range `0x20..0xFE`. |
| `0x4C` | `EOI` | WO | Write IRQ number to clear its in-service bit. |
| `0x50` | `DEFAULT` | RW | Fallback vector used when an IRQ vector is invalid. |

Requests are level-sensitive. A device holds its request active until the condition requiring service has been cleared. Masking prevents delivery but does not clear `IRR`. When the CPU accepts an IRQ, the ICU clears that pending bit and sets the corresponding `ISR` bit. The handler clears the device's interrupt condition, then writes the IRQ number to `EOI`. If the device still asserts its request after EOI, the IRQ becomes pending again. Multiple events from one source may coalesce into one bit; a device requiring event counting must provide a queue or counter.

When `IE=1`, the ICU selects the highest-priority pending, unmasked IRQ not already in service. Equal priorities are resolved in favour of the lowest IRQ number. Once an interrupt handler re-enables `IE`, only IRQs with a strictly higher priority than the highest-priority IRQ currently in service may preempt it. If no maskable IRQ is in service, any pending, unmasked IRQ may be delivered. NMI bypasses ICU masking and ordinary priority arbitration.

IRQ 0 is reserved for the PIT. Configurable peripherals use IRQ lines `1..31`, reported by their PCU descriptor. Sharing a level-sensitive line is permitted; each driver sharing a line must identify and clear its own source.

---

## Programmable Interval Timer (PIT)

The PIT provides a hardware time source independent of CPU instruction throughput. Its interval and current count are 64-bit unsigned nanosecond values.

Timer control registers at `0x00010500`. The PIT is supervisor-only; a user-mode access raises a Privilege Violation exception.

| Offset | Name | Access | Description |
|---|---|---|---|
| `0x00` | `TIMER_CTRL` | RW | Bit 0: enable. Bit 1: autoreload. Bit 2: interrupt enable. Other bits reserved. |
| `0x04` | `TIMER_INTERVAL_LO` | RW | Low 32 bits of the interval in nanoseconds. |
| `0x08` | `TIMER_INTERVAL_HI` | RW | High 32 bits of the interval. |
| `0x0C` | `TIMER_CURRENT_LO` | RO | Low 32 bits of current elapsed count. |
| `0x10` | `TIMER_CURRENT_HI` | RO | High 32 bits of current elapsed count. |
| `0x14` | `TIMER_ACK` | WO | Writing any value acknowledges the pending timer interrupt condition. |

A transition of `ENABLE` from zero to one starts a new interval with `CURRENT=0`. The count advances in nanoseconds. When a nonzero interval expires, the timer latches a pending interrupt. In one-shot mode it stops at the interval; in autoreload mode it restarts at zero and begins another interval. An interval of zero never expires. The timer continues measuring time when interrupt generation is disabled. The interrupt request remains asserted while the pending condition is set and interrupt enable is set; `TIMER_ACK` clears the pending condition.

To read `CURRENT` consistently using 32-bit accesses, read `CURRENT_HI`, then `CURRENT_LO`, then `CURRENT_HI` again; retry if the high words differ. Firmware should disable the timer while programming an interval, write both halves, acknowledge any previous request, configure control, then enable it. The PIT is connected to ICU IRQ 0.

---

## Peripheral Configuration Unit (PCU)

The PCU descriptor table is at physical address `0x00010800`. It contains 16 descriptors, each 16 bytes long. The PCU enumerates configurable peripherals only. The CPU, IHVT, ICU, PIT, MTU, and PCU itself use fixed architectural addresses and are not enumerated. The PCU is supervisor-only; a user-mode access raises a Privilege Violation exception.

| Offset | Name | Access | Description |
|---|---|---|---|
| `0x00` | Device ID | RO | 32-bit ID: Vendor (bits 31..20), Class (19..12), Device (11..4), Revision (3..0). |
| `0x04` | BAR base address | RW | Physical base address. Zero means unmapped. |
| `0x08` | Component size | RO | Number of bytes required by the device's BAR region. |
| `0x0C` | IRQ line | RO | Assigned ICU line `1..31`, or `0xFFFFFFFF` if the device has no interrupt. |

The IRQ-line field uses the formerly reserved descriptor word so the kernel can discover interrupt routing. An unimplemented descriptor reports Device ID zero, BAR zero, component size zero, and no IRQ.

Implemented BAR component sizes are nonzero powers of two. A nonzero BAR base must be aligned to its component size and the region must not wrap the 32-bit physical address space. A BAR may not overlap a fixed system region or another active BAR. The kernel may place a BAR anywhere else in physical address space, including over the nominal RAM range. While mapped, the device owns that range and the underlying RAM is unavailable; after unmapping, RAM is accessible again if present. The PCU rejects invalid assignments and leaves the previous valid mapping unchanged. A zero BAR unmaps the device.

Fixed system-address decoding takes precedence over BAR decoding, including during reset and firmware initialization. The kernel must not allocate or rely on RAM underneath an active BAR.

### PCU Device Class IDs

| ID | Meaning |
|---|---|
| `0x00` | Reserved / empty |
| `0x01` | Mass storage |
| `0x02` | Network and communications |
| `0x03` | Display and graphics |
| `0x04` | Input |
| `0x05` | System infrastructure |

Device IDs are encoded as `vendor << 20 | class << 12 | device << 4 | revision`.

---

## Physical memory map

| Start address | End address | Size | Name | Description |
|---|---|---|---|---|
| `0x00000000` | `0x00000FFF` | 4 KiB | Zero page | Permanently unmapped. Accesses fault; BARs may not claim this region. |
| `0x00001000` | `0x0000FFFF` | 60 KiB | Boot ROM | Contains reset code at `0x00001000`. |
| `0x00010000` | `0x000103FF` | 1 KiB | IHVT | Physical interrupt handler vector table. |
| `0x00010400` | `0x000104FF` | 256 B | ICU | Interrupt controller registers. |
| `0x00010500` | `0x000105FF` | 256 B | PIT | Programmable interval timer registers. |
| `0x00010600` | `0x000107FF` | 512 B | MTU | TLB control registers and suspended `USR` register portal. |
| `0x00010800` | `0x000108FF` | 256 B | PCU | 16 configurable-device descriptors. |
| `0x00010900` | `0x0001FFFF` | ~60 KiB | Unassigned | Available for BAR mappings or platform-provided memory; otherwise unmapped. |
| `0x00020000` | `0xFFFFFFFF` | Up to 4 GiB minus 128 KiB | System RAM range | RAM, except for active BAR overlays and platform-specific holes. |

There is no permanently reserved peripheral MMIO window. BARs may be assigned anywhere outside the fixed regions, including in the system RAM range, subject to PCU alignment, overlap, and address-wrap rules.

---

## Memory Translation Unit (MTU)

The MTU provides virtual-to-physical translation through a 32-entry software-managed Translation Lookaside Buffer (TLB). It has no hardware replacement policy and requires no special CPU opcodes.

### Virtual address format (32-bit, 4 KiB pages)

- Bits 31..12: Virtual Page Number (`VPN`, 20 bits)
- Bits 11..0: Byte offset (12 bits)

The current Address Space ID (`ASID`) is 10 bits (`0..1023`). Software must not reuse an ASID for another address space until old entries for that ASID have been invalidated.

When `VM=1` and `TM=0`, CPU instruction fetches and data accesses use the TLB. When `VM=0` or `TM=1`, addresses are physical and the TLB is bypassed.

### MTU control registers (`0x00010600..0x0001063F`)

| Offset | Name | Access | Description |
|---|---|---|---|
| `0x00` | `MTU_FAULT_ADDR` | RO | Address associated with the most recent CPU memory fault. Virtual while translation was active; physical in direct mode. |
| `0x04` | `MTU_CONFIG` | RW | Bits 4..0 select the TLB entry (`0..31`). Other bits reserved. |
| `0x08` | `TLB_HI_WIN` | RW | Reads/writes the selected entry's high word. |
| `0x0C` | `TLB_LO_WIN` | RW | Reads/writes the selected entry's low word. |
| `0x10` | `MTU_CURR_ASID` | RW | Current 10-bit ASID; higher bits read as zero and are ignored on writes. |
| `0x14` | `MTU_FAULT_STATUS` | RO | Access kind and reason for the most recent CPU memory fault. |

`MTU_FAULT_STATUS` bits 1..0 encode access kind: `00` instruction fetch, `01` data read, `10` data write, `11` atomic exchange. Bits 4..2 encode the reason: `000` TLB miss, `001` permission fault, `010` unmapped/bus access, `011` misalignment, `100` invalid access width. Other bits are reserved and read as zero. The fault address and status are overwritten by the next CPU memory fault. The exception vector distinguishes instruction-TLB miss, data-TLB miss, and page fault.

### Suspended USR register portal (`0x00010640..0x0001067F`)

When `PRV=1`, these 32-bit registers access the inactive user register bank. They do not expose `PC` or `FLAGS`.

| Offset | Name | Description |
|---|---|---|
| `0x40` | `USR_R0` | User `R0` |
| `0x44` | `USR_R1` | User `R1` |
| `...` | `...` | Four-byte stride per register |
| `0x78` | `USR_R14` | User `R14` |
| `0x7C` | `USR_SP` | User `R15` / stack pointer |

### TLB entry formats

`TLB_HI_WIN`:

| Bits | Field |
|---|---|
| 31..12 | Virtual Page Number (`VPN`) |
| 11..2 | Address Space ID (`ASID`, 10 bits) |
| 1 | Reserved, must be zero |
| 0 | Valid (`V`): `1` = entry active; `0` = invalid |

`TLB_LO_WIN`:

| Bits | Field |
|---|---|
| 31..12 | Physical Frame Number (`PFN`) |
| 11..4 | Reserved, must be zero |
| 3 | Writable (`W`): writes permitted when set |
| 2 | User-access gate (`R`): in user mode, all accesses require this bit; kernel mode ignores it |
| 1 | Cacheable (`C`): `1` = normal caching; `0` = uncached |
| 0 | Executable (`X`): instruction fetches permitted when set |

`R` is the user-access gate (the former `U` bit), not a separate read-permission bit. Kernel accesses ignore `R`; writes still require `W`, and instruction fetches still require `X`. In user mode, any access requires `R=1`; writes additionally require `W=1`, and instruction fetches additionally require `X=1`. MMIO mappings must set `C=0`.

An entry matches only if `V=1` and both `VPN` and `ASID` match. An invalid entry is a TLB miss. Software must not leave multiple valid entries for the same `VPN` and `ASID`; if duplicates occur, the lowest-index matching entry is used.

### Software-managed TLB replacement

The kernel selects entries by writing `MTU_CONFIG`. To install or replace an entry:

1. Select the target entry.
2. Write `TLB_HI_WIN` with `V=0` to invalidate it.
3. Write the new physical frame and permissions to `TLB_LO_WIN`.
4. Write `TLB_HI_WIN` with the new `VPN`, `ASID`, and `V=1` last.

Setting `V=1` commits the entry. Clearing `V` invalidates it. To flush the TLB, software clears `V` in every entry. The hardware never evicts entries automatically; there is no wired-entry bit.

### TLB misses and translation bypass

A TLB miss occurs when translation is enabled and no valid entry matches the requested `VPN` and `ASID`. Instruction-fetch misses raise vector `0x05`; data misses raise vector `0x06`. A valid entry whose permissions deny the access raises Page Fault vector `0x07` instead. The hardware does not refill the TLB automatically.

On a TLB miss, hardware saves the normal exception frame, enters kernel mode, disables maskable interrupts, and sets `TM=1`. While `TM=1`, instruction fetches and all CPU data accesses use physical addresses directly. The refill handler must be at a physical address and the kernel stack used by the handler must be physically accessible; the base design can use an identity-addressed kernel stack in RAM. Firmware and the OS must initialize this stack before enabling virtual memory.

The handler reads `MTU_FAULT_ADDR`, `MTU_FAULT_STATUS`, and `MTU_CURR_ASID`, obtains or constructs the mapping, installs a TLB entry, subtracts four from the saved return PC, and executes `IRET`. Subtracting four is required for instruction-fetch and data misses because the standard synchronous exception convention saves the address after the faulting instruction even though it did not complete. `IRET` restores the pre-miss `FLAGS`, including the former `TM` value.

---

## Devices

### Mass Storage Controller (Block device)

Device ID:

- Vendor `0x123`
- Class `0x01`
- Device `0x01`
- Revision `0x0`

The controller is a configurable PCU peripheral. All register offsets are relative to its assigned BAR.

| Offset | Name | Access | Description |
|---|---|---|---|
| `0x00` | `STATUS` | RO | Bit 0 `BUSY`; bit 1 `ERROR`; bit 2 completion/error interrupt pending. Other bits reserved. |
| `0x04` | `COMMAND` | WO | Write `0x1` to start READ; write `0x2` to start WRITE. |
| `0x08` | `LBA` | RW | Starting logical block address in 512-byte sectors. |
| `0x0C` | `MEM_ADDR` | RW | 32-bit physical RAM address for DMA. |
| `0x10` | `SECTOR_COUNT` | RW | Number of consecutive 512-byte sectors to transfer. |
| `0x14` | `INTERRUPT_ACK` | WO | Writing any value clears the completion/error interrupt condition. |

A transfer moves `SECTOR_COUNT * 512` bytes between disk and physical RAM beginning at `MEM_ADDR`. Device DMA uses physical addresses and does not consult the CPU TLB. Before starting, the controller validates that the full DMA range is RAM, does not wrap the 32-bit address space, and does not overlap MMIO or an active BAR. Invalid transfers fail without partial DMA. DMA cannot access CPU MMIO registers.

`BUSY` is set during a transfer. It is cleared when the transfer completes. Errors set `ERROR` and assert the device interrupt until acknowledged. A command issued while busy is rejected and sets `ERROR`. A new accepted command clears the previous error state.

### GPU (Lyra)

Device ID:

- Vendor `0x123`
- Class `0x03`
- Device `0x01`
- Revision `0x0`

The command processor and display engine share one BAR-mapped register block. Register offsets are relative to the assigned BAR.

| Offset | Name | Access | Description |
|---|---|---|---|
| `0x00` | `COMMAND_BUFFER_BASE` | RW | Physical base address of a command buffer in RAM. |
| `0x04` | `COMMAND_BUFFER_SIZE` | RW | Command-buffer size in bytes; must be a multiple of four. |
| `0x08` | `CONTROL` | RW | Writing bit 0 as one starts command execution. Other bits reserved. |
| `0x0C` | `STATUS` | RO | Bit 0 `BUSY`; bit 1 `ERROR`. |
| `0x10` | `COMPLETED_FENCE` | RO | Identifier of the latest completed GPU fence packet. |
| `0x14` | `FRAMEBUFFER_BASE` | RW | Requested physical scanout framebuffer base, latched at VBLANK. |
| `0x18` | `DISPLAY_STATUS` | RO | Bit 0 is one during vertical blank. |
| `0x1C` | `INTERRUPT_CONFIG` | RW | Bit 0 enables VBLANK, bit 1 fence-completion, bit 2 GPU-fault interrupts. |
| `0x20` | `VIDEO_MODE` | RW | `0` = display disabled; `1` = 320x240 RGB565 linear framebuffer; `0x2..0xF` reserved in this specification. |
| `0x24` | `INTERRUPT_STATUS` | RW1C | Pending VBLANK, fence, and GPU-fault bits correspond to bits 0..2. Write one to clear a pending bit. |
| `0x28..0x3F` | Reserved | -- | Must not be used by software. |

Lyra uses unified physical memory and has no virtual-memory translation of its own. Command buffers and resource data reside in system RAM. Before setting `CONTROL.START`, the CPU must make command-buffer writes globally visible, using `FENCE` where required. Command buffers are immutable while submitted and executing. The GPU executes commands in submission order.

GPU `SYNC/FENCE` packets indicate completion of preceding GPU work; they are distinct from the CPU `FENCE` instruction. The display engine reads a linear colour buffer. A write to `FRAMEBUFFER_BASE` requests a new scanout address, which is latched at VBLANK. The selected framebuffer range must be backed by RAM and must not overlap an active BAR. Invalid GPU addresses or command buffers set `STATUS.ERROR` and do not perform out-of-range memory accesses. Software must not request presentation until the GPU fence for the rendered buffer has completed. Correct fence use and VBLANK latching prevent tearing.

Lyra's shader ISA, resource model, pipeline, synchronisation, rasterisation, and precision rules are specified in the separate `gpu/` documentation. GPU resource handles are opaque IDs, not physical pointers, and are not dereferenced by shaders.

---

## System/ABI

### Calling convention

- Argument registers: `R0..R3`
- Return register: `R0`
- Callee-saved: `R4..R11`
- Caller-saved/temporaries: `R12..R14`

### Stack conventions

- Stack grows down.
- `SP` points to the next free byte.
- Word stack operations require 4-byte alignment.
- Stack is 8-byte aligned immediately before a `CALL` or `CALLR`; after the return address is pushed, the callee initially observes `SP % 8 == 4`.
- Extra arguments are placed on the stack right-to-left.
- Caller cleans up stack-passed arguments after return.

`CALL` and `CALLR` push `PC + 4`. `RET` pops the saved PC. `PUSH` and `POP`, if provided by an assembler, are pseudo-instructions rather than dedicated CPU opcodes.

### Function prologue and epilogue

A conventional prologue saves any used callee-saved registers and allocates local storage. Local-size allocation is rounded as needed to maintain the required alignment at call sites.

The epilogue restores callee-saved registers and executes `RET`.

---

## Specification notes

- This document defines the current Orion revision. Backwards compatibility between revisions is not required.
- Fixed system registers are always decoded ahead of peripheral BAR mappings.
- Firmware initializes state that is unspecified at reset before relying on it.
- Lyra's detailed graphics architecture remains in `gpu/`; the device register block above is the CPU-visible interface for this revision.
