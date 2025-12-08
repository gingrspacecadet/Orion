# Overview

Orion is an operating-system project built on a custom 32-bit word-oriented CPU architecture and a small emulator. This documentation is generated from the repository sources (`emu/`, `asm/`, `bios/`, `kernel/`, `test/`) and is intended to be complete and actionable for contributors.

Key points:
- 32-bit instruction words, 16 general-purpose registers (`R0..R15`).
- Memory: word-addressable RAM and a ROM for BIOS images.
- Modes: `BIOS`, `KERNEL`, `USER`.
- Devices: VGA (framebuffer), keyboard, block device (disk image `orion.img`).
- Tooling: a small assembler (`asm/main.c`) and emulator (`emu/`), with `Makefile` targets to build and run.

Where to start:
- Quick build: see `BuildAndRun.md`.
- Read `Architecture.md` for machine model and execution semantics.
- See `ISA.md` and `Encoding.md` for instruction reference and bit layouts.
