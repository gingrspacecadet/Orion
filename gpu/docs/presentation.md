# Lyra Presentation System

## Overview

The Lyra presentation system defines how completed framebuffers are scanned out to a display.

It is designed to be deterministic, double-buffered, and tightly coupled to fence completion semantics.

There is no implicit compositor or window system inside the GPU.

---

## Display Engine Model

The display engine is a fixed-function unit responsible for:

- reading a framebuffer from memory
- scanning pixels linearly to output
- synchronising frame swaps with VBLANK
- optionally generating interrupts

---

## Framebuffer Model

A framebuffer is a linear memory region containing:

- colour buffer (required)
- depth buffer (optional, not scanned out)

Only colour data is visible to the display engine.

---

## Supported formats

Base specification:

- RGBA8
- RGB565 (optional mode)

No floating-point scanout formats.

---

## Scanout behaviour

The display engine reads pixels sequentially:

```

top-left → top-right
row by row

```

No tiling or swizzling is applied at scanout time.

---

## Resolution model

The active display mode is set via the GPU display register.

Typical supported modes:

- 320×240
- 640×480
- 1280×720
- 1920×1080 (target mode)

Resolution changes only take effect at VBLANK boundary.

---

## Double buffering

Lyra uses strict double buffering.

There are two framebuffers:

- front buffer (currently displayed)
- back buffer (currently rendered into)

---

## Buffer swap rules

A buffer swap occurs only when:

1. GPU has completed rendering to back buffer
2. associated fence has been signalled
3. display engine reaches VBLANK

Swap is then performed atomically.

No partial swaps are permitted.

---

## VBLANK model

The display operates on a fixed refresh loop.

During VBLANK:

- scanout is paused
- framebuffer pointer may be updated
- swap is committed

Outside VBLANK:

- framebuffer memory is read-only from display perspective

---

## Synchronisation with GPU

Frame presentation is tied to fences:

- CPU submits render commands
- GPU signals completion via fence
- only then can framebuffer be queued for presentation

This guarantees:

- no tearing
- no partially rendered frames
- deterministic frame boundaries

---

## Presentation queue

The display engine maintains a small queue:

```

Frame N → Frame N+1 → Frame N+2


```

Only completed frames (fence-signalled) may enter the queue.

---

## Interrupt model

The display engine may optionally generate:

* VBLANK interrupt

This is routed through the ICU and behaves like a standard hardware interrupt.

Use cases:

* frame pacing
* CPU-GPU synchronisation
* timing control

---

## Memory visibility rules

A framebuffer becomes visible to the display engine only when:

* GPU has completed all writes
* fence has been signalled
* buffer is committed during VBLANK

CPU writes after submission are undefined behaviour.

---

## Tearing prevention guarantee

Lyra guarantees:

> no tearing under correct fence + VBLANK synchronisation

Tearing only occurs if:

* CPU ignores fence completion rules
* or writes directly to active scanout buffer

---

## Latency model

Typical pipeline:

```
CPU submits frame N
GPU renders frame N
Fence N signals completion
VBLANK occurs
Frame N becomes visible
```

This results in:

* 1-frame minimum latency under ideal conditions
* deterministic frame pacing when properly synchronised

---

## Multi-buffering extension (optional)

Future extensions may allow:

* triple buffering
* frame queue depth > 2

But base specification is strictly double-buffered.

---

## Determinism guarantee

Given identical:

* command buffers
* fence usage
* framebuffer addresses
* display mode configuration

The presented output must be identical across:

* emulator
* silicon implementation

---

## Depth Buffer Format

The depth buffer uses FP24 values.

The recommended convention is reverse-Z:

    Near plane = 1.0
    Far plane = 0.0

Depth comparison is typically:

    GREATER

Reverse-Z improves depth precision near the camera and allows large view distances with reduced Z-fighting.