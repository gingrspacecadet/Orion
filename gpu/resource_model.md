# Lyra Resource Model

## Overview

Lyra uses a fully explicit resource model. All GPU resources are created, referenced, and destroyed through the CPU-side API and accessed via opaque 32-bit handles in command buffers.

There is no implicit allocation, no garbage collection, and no virtual GPU memory.

---

## Core principles

- all resources are explicitly created and destroyed
- all GPU memory is physically backed
- all bindings are explicit in command buffers
- no hidden residency management
- deterministic resource lifetime

---

## Resource types

Lyra defines the following GPU resources:

- buffers
- textures
- framebuffers
- shader binaries
- pipeline objects

Each is identified by a 32-bit handle.

---

## Handle model

```
uint32 handle
```

Handles are opaque indices into driver-managed resource tables.

Handles are never dereferenced by the shader core.

---

## Buffers

### Definition

Buffers are linear blocks of GPU-accessible memory.

Used for:

* vertex data
* index data
* uniform/constant uploads (CPU-side staging)
* compute-like data streams (future extension)

---

### Buffer layout

Buffers are byte-addressed:

```
address = base + offset
```

No alignment is required by hardware except where specified by pipeline state.

---

### Buffer usage types

| Type     | Meaning                 |
| -------- | ----------------------- |
| VERTEX   | vertex attribute source |
| INDEX    | index stream            |
| CONSTANT | CPU-uploaded constants  |
| STREAM   | generic data            |

---

## Textures

Textures are immutable or semi-immutable image resources.

They contain:

* format
* width
* height
* mip levels
* base memory address

Textures are sampled only via texture units.

No direct memory access exists in shaders.

---

## Texture lifetime rules

* textures are created before use
* textures may be updated only via explicit CPU upload
* no partial GPU writes to textures (base spec)

---

## Framebuffers

A framebuffer is a collection of render targets.

Components:

* colour attachment(s)
* depth attachment
* optional stencil attachment (future extension)

Framebuffer binding is required before any draw call.

---

## Shader resources

Shader binaries are immutable resources containing:

* compiled LASM instruction stream
* metadata (input/output signature)

Shaders are not modified after creation.

---

## Pipeline objects

Pipeline objects are immutable bundles of:

* vertex shader
* fragment shader
* raster state
* depth state
* blend state
* vertex layout

Binding a pipeline fully configures GPU fixed-function state.

---

## Resource creation model

All resources are created via CPU-side driver calls:

Conceptually:

```
create_buffer(...)
create_texture(...)
create_pipeline(...)
load_shader(...)
```

Each returns a 32-bit handle.

---

## Resource binding model

Binding is explicit in command buffers:

Examples:

* SET_VERTEX_BUFFER
* SET_TEXTURE
* SET_PIPELINE
* BIND_RENDER_TARGET

No implicit global state exists outside the command stream.

---

## Resource lifetime rules

### Ownership

* CPU owns creation and destruction
* GPU consumes resources asynchronously

---

### Safety rule

A resource must remain valid until:

* GPU has passed a fence signalling completion of all commands referencing it

---

### Destruction

Destroying a resource while in use results in undefined behaviour and is forbidden.

---

## Synchronisation dependency

Resource lifetime correctness depends on fences:

* CPU must not reuse or free resources before GPU completion
* GPU signals completion via fence packets

---

## Memory model

All resources reside in a unified physical address space:

* no GPU virtual memory
* no paging
* no demand loading

Memory is explicitly allocated by the driver layer.

---

## Residency model

There is no automatic residency management.

If memory is insufficient:

* resource creation fails

---

## Upload Model

No dedicated upload mechanism exists.

The CPU writes resource contents directly into system memory.

Once a resource has been created and populated, the GPU accesses it directly.

No copies between CPU memory and VRAM occur because Lyra uses unified memory.

---

## Determinism guarantee

Given identical:

* resource creation order
* command buffer contents
* shader binaries
* pipeline state

The GPU must produce identical results across:

* emulator
* silicon implementation

---

## Memory Architecture

Lyra uses a unified physical memory architecture.

The CPU and GPU share the same physical address space.

There is no dedicated VRAM and no separate GPU address space.

All resources ultimately reside in system RAM.

The GPU accesses resource memory directly.

---

## Resource Backing

Buffers, textures, framebuffers, shader binaries, and pipeline objects are physically backed by memory in system RAM.

Resource handles are opaque identifiers managed by the driver and do not correspond directly to memory addresses.

Internally, the GPU maintains descriptor tables describing each resource.
