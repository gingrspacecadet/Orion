# Emulator

Location: `emu/` — core emulator implementation and device drivers.

Entry point: `emu/main.c`.

Behavior and main loop
- The emulator loads a program (array of `uint32_t` words) into memory via `bus_write`.
- If a BIOS path is provided as the second command-line argument, it is loaded into `Machine.rom` and `mode` is set to `BIOS`.
- `cpu.sp` is initialized to `RAM_SIZE` and `cpu.pc` to `0`.
- The main loop calls `step()` while `cpu.running` is true. `step()` increments cycle count, checks interrupts, fetches an instruction and dispatches via `ops[]`.

Opcode dispatch
- `ops[]` table (defined in `emu/main.c`) maps numeric opcode indices to handler functions implemented in `emu/ops.h`.
- Handlers are declared with macro `OP(name)` and operate on `(Machine* m, uint32_t op)`.

Debugging
- When built with `-DDEBUG`, the emulator provides a step-by-step terminal UI with live CPU state printing and signal handlers (see `emu/main.c`).

Memory and devices
- Memory access generally goes through the bus (`bus_read`/`bus_write`) which delegates to registered device handlers or the RAM fallback.
- Devices register via `bus_register(&m, &device)` and implement a `Device` interface (`read`, `write`, `init`).
