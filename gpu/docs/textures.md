# Lyra Texture System

## Overview

The Lyra texture system is fixed-function and designed for deterministic sampling with minimal hardware complexity.

It does not use virtual memory or shader-visible cache control.

---

## Texture Formats

Supported formats:

- RGBA8
- RGB565
- RGBA4
- A8

Optional extension formats may be defined later.

---

## Addressing Model

Textures use physical addressing only.

A texture is defined by:

- base address
- width
- height
- mipmap offsets

No virtual address translation is performed.

---

## Texture Coordinates

Texture coordinates are provided in FP24 format in shaders.

Internally, they are converted to fixed-point format for sampling:

Recommended internal format:
16.12 or 20.12 fixed-point

---

## Sampling Modes

Each texture unit supports:

- nearest filtering
- bilinear filtering

No trilinear or anisotropic filtering in the base specification.

---

## Mipmapping

Mipmaps are stored linearly in memory per level.

LOD selection is computed using a fixed-function approximation based on screen-space derivatives.

---

## Texture Units

Lyra provides 8 texture units (TU0–TU7).

Each unit contains:
- sampler state
- address generator
- cache
- filtering hardware

Units operate independently.

---

## TEX instruction

The TEX instruction performs a texture lookup:
```
TEX Rdest, Rcoord.xy, Tunit
```
Behaviour:
- fetch texel(s) from selected texture unit
- apply filtering
- return FP24 colour result to destination register