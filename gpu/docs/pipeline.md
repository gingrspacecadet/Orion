# Lyra Pipeline State Objects

## Overview

Pipeline objects define all fixed-function GPU state required for rendering. Pipelines are immutable once created.

Binding a pipeline changes all rasterisation and shading behaviour.

---

## Pipeline Structure
```
Pipeline {
    Vertex Shader ID
    Fragment Shader ID
    Raster State
    Depth State
    Blend State

    Vertex Layout
}
```

---

## Pipeline ID

Pipelines are referenced by a 32-bit handle.

Creation is assumed to be handled by the driver or CPU-side API layer.

---

## Vertex Layout

Defines how vertex buffers are interpreted.

Example:
```
Position: vec3 FP24
Normal: vec3 FP24
UV0: vec2 FP24
Colour: FGBA8
```

Layout is fixed per pipeline.

---

## Raster State

Controls primitive setup.

Fields:

- cull mode (none / back / front)
- front face winding
- scissor enable
- tile size hint (implementation-defined)

---

## Depth State

Fields:

- depth test enable
- depth function:
  - LESS
  - LEQUAL
  - GREATER
  - GEQUAL
  - ALWAYS
  - NEVER
- depth write enable

Depth buffer is always 24-bit integer.

---

## Blend State

Defines framebuffer blending:

Modes:

- NONE
- ALPHA
- ADDITIVE
- SUBTRACTIVE

Blend equation is fixed-function.

---

## Vertex Shader Binding

Each pipeline binds a vertex shader:

Constraints:

- SIMD8 execution model
- FP24 arithmetic only
- no branching (predicate-only execution)

---

## Fragment Shader Binding

Each pipeline binds a fragment shader:

Inputs:
- interpolated varyings
- FRAGCOORD
- depth

Outputs:
- COLOR0
- DEPTH (optional)

---

## Pipeline Immutability

Once created, pipelines cannot be modified.

This ensures:

- deterministic execution
- cacheable state
- fast binding in command stream