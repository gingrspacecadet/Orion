# Lyra Graphics Architecture (LGA)

## Purpose

Lyra is a deterministic, tile-based programmable GPU designed for real-time 3D rendering at approximately 1080p60. It targets PS2/PS3-era rendering complexity with a simpler, silicon-feasible execution model and a Vulkan-inspired command submission interface.

The design prioritises:

- deterministic execution
- predictable memory behaviour
- low hardware complexity
- high emulation fidelity
- explicit control over performance trade-offs

## Non-goals

Lyra does not attempt to provide:

- full modern GPU feature parity
- virtual memory in the GPU core
- out-of-order execution or dynamic scheduling
- IEEE-754 floating-point compliance
- general-purpose compute as a primary design goal

## High-level model

Lyra is a command-driven GPU. The CPU builds command buffers which are executed asynchronously by the GPU. All rendering is performed through a fixed pipeline with programmable vertex and fragment stages.

## Core properties

- SIMD8 shader execution model
- vec4-based register architecture
- FP24 native arithmetic format (non-IEEE)
- tile-based rasterisation
- fixed-function raster backend
- programmable vertex and fragment shaders
- explicit pipeline state objects
- deterministic execution model