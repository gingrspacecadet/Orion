# Lyra Architecture

## Pipeline Overview

Lyra executes rendering in the following fixed sequence:

CPU
-> Command Processor
-> Vertex Shader
-> Primitive Assembly
-> Clipping
-> Viewport Transform
-> Tile Binning
-> Rasterisation
-> Perspective-Correct Interpolation
-> Fragment Shader
-> ROP
-> Framebuffer Output
-> Display Engine

Execution is strictly ordered. There is no hardware reordering.

---

## Shader Execution Model

Shaders execute in SIMD8 groups. All lanes execute the same instruction simultaneously.

There is no dynamic scheduling or out-of-order execution.

---

## Register Architecture

### General Registers

32 vector registers:

R0–R31

Each register contains:

vec4 FP24 = (x, y, z, w)

---

### Predicate Registers

P0–P7

Each predicate is a single bit used for predicated execution. Lyra uses predication instead of branching.

---

### Constant Registers

C0–C255

Read-only registers loaded by the command processor prior to shader execution.

Typical usage:
- transformation matrices
- lighting parameters
- material constants
- skeletal animation data

---

## Numeric Format: FP24

Lyra uses a custom 24-bit floating-point format.

Format:

- 1 sign bit
- 7 exponent bits
- 16 mantissa bits

Rules:

- no NaN values
- no infinity values
- denormals are flushed to zero
- overflow is saturated
- single deterministic rounding mode is used

This format applies to all shader arithmetic unless otherwise specified.

---

## Coordinate and Precision Domains

Different pipeline stages use different numeric representations:

| Stage | Representation |
|------|----------------|
| Vertex shader | FP24 |
| Clip space | FP24 |
| Screen space | 16.8 fixed-point |
| Edge functions | integer arithmetic |
| Interpolation | FP24 |
| Fragment shader | FP24 |
| Depth buffer | 24-bit unsigned integer |
| Colour buffer | 8-bit per channel formats |

---

## Rasterisation Model

### Triangle setup

Triangles are processed using edge functions of the form:

E(x, y) = Ax + By + C

Edge coefficients are computed per triangle.

---

### Coverage rule

Lyra uses the top-left rule for rasterisation. This ensures deterministic coverage and prevents cracks between adjacent primitives.

A pixel is covered if:

E0(x, y) ≥ 0
E1(x, y) ≥ 0
E2(x, y) ≥ 0

Pixel centres are defined at:

(x + 0.5, y + 0.5)

---

### Screen-space precision

Screen-space positions are stored in 16.8 fixed-point format to ensure subpixel accuracy and stable rasterisation.

---

## Perspective-Correct Interpolation

Lyra performs perspective-correct interpolation for all varyings.

For each vertex:

attribute_w = attribute / w
inv_w = 1 / w

The rasteriser interpolates both values across the triangle and reconstructs:

attribute = interpolated(attribute_w) / interpolated(inv_w)

All interpolation is performed in FP24.

---

## Tile-based Rasterisation

Rendering is performed using a tile-based deferred rasteriser.

Recommended tile size: 32×32 pixels

Process:
- triangles are bin-sorted into tiles
- each tile is rasterised independently
- tile-local memory is used where possible

Tiles are not exposed to the API.