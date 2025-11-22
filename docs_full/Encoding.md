# Encoding & Bit Layout

Bit numbering: MSB = 31, LSB = 0.

Helpers (from `emu/machine.h`):
- `getbits(v, hi, lo)` — extract inclusive bit range.
- `getbyte(target, start)` — extract the byte ending at `start` (i.e. `getbits(target, start, start-8)`).
- `getbit(target, bit)` — extract single bit.
- `sign_extend(value, bits)` — sign-extend a `bits`-wide value to 32 bits.

Common field layout (used by many instructions):
- `opcode`: bits `31..26` (6 bits)
- `dest`  : bits `25..22` (4 bits)
- `src1`  : bits `21..18` (4 bits)
- `src2/imm`: bits `17..2` (4-bit register index or 16-bit immediate when I-type)

Assembler notes:
- The assembler writes `((uint32_t)opcodes[index].opcode << 24)` which places the 6-bit opcode in the top byte; the emulator uses `getbyte(op, 32) >> 2` to recover the opcode index.
- The `.str` directive packs ASCII characters into little-endian 32-bit words (4 characters per word).
