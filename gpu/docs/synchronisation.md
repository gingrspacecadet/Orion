# Lyra Synchronisation Model

## Overview

Lyra uses a strict, explicit synchronisation model based on fences and command stream ordering.

There is no implicit synchronisation between CPU and GPU beyond command buffer submission order.

There is no speculative execution model exposed to software.

---

## Core principles

- GPU executes commands in submission order
- CPU and GPU operate asynchronously
- synchronisation is explicit only
- no hidden cache coherency guarantees beyond specification
- all hazards are resolved by programmer-visible fences

---

## Execution ordering

Within a single command buffer:

- commands execute strictly in order
- state changes are immediately visible to subsequent commands
- there is no reordering of draw calls or state changes

Across command buffers:

- execution order is defined by submission order unless explicitly synchronised otherwise

---

## Fence model

### Fence definition

A fence is a 32-bit value associated with a GPU submission point.

```
uint32 fence_id
```

---

### Fence creation

Fences are inserted into command buffers via:

```
SYNC / FENCE packet
```

When encountered, the GPU records:

- all previous work must complete before fence is signalled

---

### Fence signalling

A fence is signalled when:

- all prior commands in submission order are fully executed
- all memory writes from those commands are visible to CPU

---

### CPU wait

The CPU may query or block on fences:

- poll fence completion
- or block until fence is signalled

---

## Memory visibility model

Lyra uses a strongly ordered memory model between CPU and GPU with explicit boundaries.

### GPU write visibility

GPU writes become visible to CPU only after:

- corresponding fence is signalled

### CPU write visibility

CPU writes become visible to GPU only after:

- command buffer submission point that occurs after write

---

## Hazard classes

### 1. Read-after-write (RAW)

Handled implicitly within GPU pipeline ordering.

No additional synchronisation required inside a single command buffer.

---

### 2. Write-after-read (WAR)

Forbidden unless separated by a fence.

Example:

- rendering to texture
- then reading same texture without sync

---

### 3. Write-after-write (WAW)

Ordering is preserved by submission order.

No reordering occurs.

---

## Resource lifetime dependency

Resources must remain valid until:

- all fences referencing those resources are signalled

Failure to respect this rule results in undefined behaviour.

---

## Double buffering model

Recommended presentation model:

- buffer A rendered while buffer B displayed
- swap buffers after fence completion

This avoids stalling GPU pipeline.

---

## Command buffer submission ordering

The GPU maintains a submission queue:

```

Queue:
CB0 → CB1 → CB2 → ...

```

Execution is strictly FIFO unless interrupted by synchronisation barriers.

---

## GPU pipeline hazards

The GPU does not perform automatic hazard tracking across command buffers.

Only the following are guaranteed:

* intra-command-buffer ordering
* fence-based inter-buffer ordering

---

## Synchronisation packet behaviour

When a SYNC/FENCE packet is encountered:

1. GPU completes all prior commands
2. internal state is flushed
3. fence is marked pending
4. CPU-visible completion occurs after full pipeline drain

---

## Display synchronisation

Display engine operates independently but reads only from:

* completed framebuffer writes
* last signalled fence-safe buffer

No tearing control is implicit.

VSync behaviour is implementation-defined but typically:

* framebuffer swap occurs only after fence completion

---

## Determinism guarantee

Given identical:

* command buffer order
* fence placement
* resource lifetimes

The GPU must produce identical results across:

* emulator
* silicon implementation
