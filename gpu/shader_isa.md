# Lyra Shader Binary Format

## Overview

Lyra shaders are compiled into a compact 64-bit instruction format.

Execution is SIMD8 over vec4 FP24 registers.

Shaders are linear instruction streams with optional predication.

There is no branching, no call stack, and no dynamic control flow.

---

## Shader Types

Two shader stages exist:

- Vertex Shader
- Fragment Shader

Both use identical ISA.

Stage is defined by pipeline binding, not instruction differences.

---

## Instruction Width

All instructions are 64-bit fixed width.

This guarantees:

- single-cycle decode (conceptually)
- simple instruction fetch
- predictable alignment
- easy emulation

---

## Register Model

### Vector Registers

```

R0–R31

```

Each is:

```

vec4 FP24

```

---

### Predicate Registers

```

P0–P7

```

Used for conditional execution masking.

---

### Constant Registers

```

C0–C255

```

Read-only.

---

## Instruction Format (64-bit)

```

bits 63–58 : opcode (6 bits)
bits 57–55 : condition (3 bits)
bits 54–52 : predicate register (3 bits)

bits 51–47 : destination register (5 bits)

bits 46–34 : source A descriptor (13 bits)

bits 33–21 : source B descriptor (13 bits)

bits 20–8  : source C descriptor (13 bits)

bits 7–0   : modifier / immediate / extension

```

Notes:

* Source C is optional depending on opcode
* unused fields are ignored per opcode definition

---

## Condition Field

Controls predicated execution:

| Value | Meaning |
| ----- | ------- |
| 0x0   | ALWAYS  |
| 0x1   | EQ      |
| 0x2   | NE      |
| 0x3   | LT      |
| 0x4   | LE      |
| 0x5   | GT      |
| 0x6   | GE      |
| 0x7   | NEVER   |

Condition is evaluated against predicate register selected in bits 54–52.

If condition fails, instruction becomes NOP.

---

## Source Descriptor Format (13-bit)

Each source operand (A/B/C) is encoded as:

```
bit 12     : immediate flag
bit 11     : constant flag
bits 10–8  : swizzle X
bits 7–5   : swizzle Y
bits 4–2   : swizzle Z
bits 1–0   : swizzle W (partial / reserved extension)
```

---

## Swizzle Encoding

Each component selector:

```
00 = x
01 = y
10 = z
11 = w
```

Example:

```
.yxxx
= 01 00 00 00
```

---

## Immediate Mode

If immediate flag is set:

* source is a 16-bit signed FP24-compatible constant
* broadcast to (x,y,z,w)

Typical uses:

* 0
* 1
* -1
* 0.5

No full-width immediates exist.

---

## Constant Mode

If constant flag is set:

* source refers to C0–C255
* index encoded in low bits of descriptor

---

## Opcode Table (Core ISA)

### Arithmetic

| Opcode | Mnemonic | Behaviour      |
| ------ | -------- | -------------- |
| 0x00   | NOP      | no operation   |
| 0x01   | MOV      | rd = a         |
| 0x02   | ADD      | rd = a + b     |
| 0x03   | SUB      | rd = a - b     |
| 0x04   | MUL      | rd = a * b     |
| 0x05   | MAD      | rd = a * b + c |

---

### Math

| Opcode | Mnemonic |
| ------ | -------- |
| 0x06   | MIN      |
| 0x07   | MAX      |
| 0x08   | ABS      |
| 0x09   | NEG      |
| 0x0A   | DP3      |
| 0x0B   | DP4      |
| 0x0C   | CROSS    |
| 0x0D   | RCP      |
| 0x0E   | RSQ      |
| 0x0F   | SQRT     |

---

### Comparison / Selection

| Opcode | Mnemonic |
| ------ | -------- |
| 0x10   | CMPEQ    |
| 0x11   | CMPNE    |
| 0x12   | CMPLT    |
| 0x13   | CMPLE    |
| 0x14   | CMPGT    |
| 0x15   | CMPGE    |
| 0x16   | SEL      |

---

### Graphics

| Opcode | Mnemonic |
| ------ | -------- |
| 0x17   | TEX      |
| 0x18   | M4X4     |

#### M4X4 Semantics
`M4X4` implicitly consumesa contiguous block.

Example:
```
M4X4 rD, rS, cN
```
Expands to:
```
rD.x = dot(rS, cN)
rD.y = dot(rS, cN+1)
rD.z = dot(rS, cN+2)
rD.w = dot(rS, cN+3)
```

Each matrix is stored as:
- 4 consecutive vec4 constants
Interpretation depends on convention, but fixed ordering is required.

---

### Control

| Opcode | Mnemonic |
| ------ | -------- |
| 0x19   | END      |

---

## Predicate Execution Model

Each instruction executes only if:

```
(P[p] AND condition_result) == TRUE
```

Where:

* p is predicate register index
* condition_result is comparison outcome

If false:

* instruction is treated as NOP
* no side effects occur

---

## SIMD Execution Model

Each instruction operates over SIMD8 lanes:

* all lanes execute same opcode
* swizzles apply per lane
* predicates mask lanes uniformly

There is no divergence.

---

## Register Write Model

Writes can be masked:

* full vec4 write
* partial component write (x/y/z/w enable mask)

Unwritten components remain unchanged.

---

## Pipeline Binding

Shader binaries are bound into pipelines via:

```
VertexShaderID
FragmentShaderID
```

Shader binaries are immutable once loaded.
