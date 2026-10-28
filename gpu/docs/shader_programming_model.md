# Lyra Shader Programming Model

## Overview

Lyra shaders are written in a low-level assembly language called LASM (Lyra Assembly Shader Model).

LASM is a direct textual representation of the 64-bit shader binary ISA.

There is no high-level shading language in the base specification.

---

## Design goals

- one-to-one mapping to binary ISA
- deterministic compilation
- no undefined evaluation order
- explicit register allocation
- explicit pipeline I/O mapping
- easy to assemble and disassemble

---

## Shader types

Two shader entry types exist:

- vertex
- fragment

Both use identical instruction set.

Shader type is defined in pipeline binding, not in code.

---

## File format

A shader file contains:

```text id="f1"
.shader "name"
.stage vertex|fragment
```

Followed by instruction lines.

---

## Registers

### Vector registers

```
r0–r31
```

Each is:

```
vec4 fp24
```

---

### Predicate registers

```
p0–p7
```

---

### Constants

```
c0–c255
```

Read-only.

---

## Instruction syntax

General form:

```
OP dst, srcA, srcB, srcC
```

Not all operands are required.

---

## Examples

### Move

```
MOV r0, r1
```

---

### Add

```
ADD r0, r1, r2
```

---

### Multiply-add

```
MAD r0, r1, r2, r3
```

---

### Dot product

```
DP3 r0, r1, r2
DP4 r0, r1, r2
```

---

### Texture sample

```
TEX r0, r1.xy, c5
```

Meaning:

* r1.xy = UV coordinate
* c5 = texture unit binding index

---

### Conditional execution

```
(C0) ADD r0, r1, r2
```

Means:

* execute only if predicate C0 is true

No branching exists.

---

## Swizzling

Format:

```
r1.xyzw
r1.xxxx
r1.zyxw
```

Allowed components:

* x
* y
* z
* w

---

## Write masks

```
MOV r0.xyw, r1
```

Only selected components are written.

Unwritten components remain unchanged.

---

## Immediate values

Immediate constants are prefixed with `#`:

```
ADD r0, r1, #1.0
```

Immediates are encoded as FP24 constants by the assembler.

Allowed range is limited to representable FP24 values.

---

## Constant binding

Constants are referenced explicitly:

```
c0–c255
```

Example:

```
M4X4 r0, r1, c0
```

---

## Pipeline I/O conventions

### Vertex shader inputs

Implicit registers:

```
i0 = position
i1 = normal
i2 = uv0
i3 = colour
```

Mapped by vertex layout in pipeline.

---

### Vertex shader outputs

Must write:

```
o0 = clip position (required)
o1–o15 = varyings (optional)
```

---

### Fragment shader inputs

Automatically interpolated:

```
v0–v15
fragcoord
depth
```

---

### Fragment outputs

```
o0 = colour
o1 = depth (optional)
```

---

## Compilation model

LASM is compiled in two phases:

### 1. Assembly phase

* parses instruction text
* resolves registers
* resolves labels (future extension)
* validates operand legality

### 2. Encoding phase

* converts instructions into 64-bit binary format
* applies swizzle encoding
* encodes predicates and immediates

Output is a shader binary blob.

---

## No undefined behaviour rules

The assembler must enforce:

* all registers must be declared valid
* no out-of-range constants
* no invalid swizzle components
* no invalid write masks
* all predicate usage must be explicit

Invalid shaders are rejected at compile time.

---

## Linking model

Shaders are standalone binaries.

They are not dynamically linked.

Pipeline binds:

* vertex shader binary ID
* fragment shader binary ID

No cross-shader function calls exist.

---

## Determinism guarantee

Given identical inputs:

* same shader binary
* same pipeline state
* same constants

Execution must be bit-identical across:

* emulator
* silicon implementation

---

## M4X4 Instruction

The M4X4 instruction performs a 4×4 matrix multiplication.

Syntax:

    M4X4 rd, rs, cN

The source vector rs is multiplied by four consecutive constant registers.

The operation is defined as:

    rd.x = dot(rs, cN)
    rd.y = dot(rs, cN+1)
    rd.z = dot(rs, cN+2)
    rd.w = dot(rs, cN+3)

Matrices are stored in row-major order.

cN through cN+3 must all exist.

No strided or indirect matrix addressing is supported.