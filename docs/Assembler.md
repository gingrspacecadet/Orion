# Assembler

Location: `asm/main.c` — a small single-file assembler used to translate `.s` sources to binary `.out` (32-bit words).

Highlights
- Opcode table: `asm/ops.h` defines the opcode entries (`name`, `type`, `opcode`, `num_operands`). Types are `I`, `R`, `RI`, `M`.
- The assembler reads input lines, records labels (tokens starting with `$`) in a label table, and encodes instructions based on the opcode `type`.
- Directives: `.str` packs a string into little-endian 32-bit words (4 chars per word).

Encoding behavior
- The assembler writes the opcode into the high bits: `((uint32_t)opcodes[index].opcode << 24)`.
- Different `type` flows handle registers, immediates and masks; `RI` encodes either register operands or immediate forms by setting a low bit for I-type forms.

Errors & messages
- The assembler performs validation (register names `R0..R15`, immediate formats like `#42` or `#0x2A`) and reports user-friendly errors with colored output.
