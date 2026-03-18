# Devices

Devices live under `emu/devices/` and implement the `Device` interface from `emu/device.h` (callbacks `read`, `write`, `init`, plus `base`, `size`, `state`).

Built-in devices
- `vga` (`emu/devices/vga.c`, `vga.h`): framebuffer / text output device. Registered from `emu/main.c`.
- `keyboard` (`emu/devices/keyboard.c`, `keyboard.h`): keyboard input device that can push characters and synthesize an interrupt.
- `block` (`emu/devices/block.c`, `block.h`): block device backed by disk image `orion.img` (opened with `block_init("orion.img")`).

Bus semantics
- `bus_register` stores device pointers into an internal `DeviceManager` and calls `dev->init`.
- `bus_read`/`bus_write` iterate registered devices and dispatch reads/writes to the matching device address range (fall back to RAM when no device matches).

Extending devices
- Implement the `Device` structure, provide `read` and/or `write`, choose a `base` and `size`, then call `bus_register(&m, &your_device)` in `emu/main.c` or during runtime initialization.
