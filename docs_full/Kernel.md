# Kernel Example

Location: `kernel/main.s` — small kernel example assembled by the project assembler.

What it does
- Writes several words to a memory-mapped device region (example using `mov` and `str`) to initialize a device-driven message.
- Enters an infinite loop at `.loop` to simulate a simple kernel idle loop.

Usage
- Build with `make kernel` which invokes the assembler to produce `kernel.out`.
- Run `./build/orion kernel.out bios.out` to start the emulation with both kernel and BIOS images.
