# BIOS

Location: `bios/main.s` — example BIOS code assembled by `build/asm`.

Behavior
- The BIOS examples define interrupt handlers (`TIMER_HANDLER`, `IRQ1_HANDLER`) and a `BOOT` entry that initializes state and sets interrupt vectors.
- The BIOS is loaded into `Machine.rom` when a BIOS path is provided to the emulator; while `Machine.mode == BIOS` `fetch()` reads instructions from the `rom`.

Interaction with Kernel
- `INT` in kernel mode pushes PC, reads handler pointer from a specific bus address and switches to BIOS mode; `IRET` returns from BIOS to kernel restoring PC and flipping mode.
