# Instruction Set (ISA)

This page summarizes opcodes declared in `asm/ops.h` and implemented in `emu/ops.h`.

Template for entries:
- **Mnemonic** — opcode value (binary)
- **Type** — `R`, `I`, `RI`, `M`
- **Fields** — bit positions and semantics
- **Semantics** — pseudo-code

Known opcodes (short list from `asm/ops.h`):

- `NOP`  — `0b00000000` — Type: `R` — no-op.
- `MOV`  — `0b00000100` — Type: `RI` — register-to-register move or load immediate.
- `ADD`  — `0b00001000` — Type: `RI` — add (register or immediate).
- `SUB`  — `0b00001100` — Type: `RI` — subtract.
- `OR`   — `0b00010100` — Type: `RI` — bitwise OR.
- `SHL`  — `0b00011100` — Type: `RI` — shift-left (register or immediate shift).
- `LDR`  — `0b00101000` — Type: `I` — load from memory: `R[dest] = M[R[base] + imm]`.
- `STR`  — `0b00101100` — Type: `I` — store to memory: `M[R[base] + imm] = R[src]`.
- `CMP`  — `0b00110000` — Type: `RI` — compare and set `F_ZERO`.
- `JMP`  — `0b00110100` — Type: `M` — PC-relative jump.
- `JE`   — `0b00111100` — Type: `M` — jump if zero.
- `JNE`  — `0b01000000` — Type: `M` — jump if not-equal.
- `PUSH` — `0b01010100` — Type: `I` — push set of registers by mask.
- `POP`  — `0b01011000` — Type: `I` — pop into registers by mask.
- `HLT`  — `0b01011100` — Type: `R` — halt CPU (`cpu.running = false`).
- `INT`  — `0b01111100` — Type: `I` — software interrupt.
- `CALL` — `0b10000000` — Type: `M` — push PC and jump.
- `RET`  — `0b10000100` — Type: `R` — pop PC.
- `IRET` — `0b10001000` — Type: `R` — return from interrupt (restore mode and PC via `pop`).

See `emu/ops.h` for implementation details and pseudo-code of each handler.
