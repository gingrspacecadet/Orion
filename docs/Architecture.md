# Architecture

**Machine model**
- 32-bit word architecture.
- 16 general-purpose registers stored in `Machine.cpu.registers[16]`.
- `CPU` shape (fields): `cycle`, `pc`, `sp`, `registers[16]`, `interrupt` (uint16_t), `flags` (uint8_t), `running` (bool).
- The outer `Machine` contains the `CPU`, `mode` (enum: `BIOS`, `KERNEL`, `USER`), and pointers to `ram` and `rom` arrays.

**Flags**
- Defined in `emu/machine.h`: `F_ZERO`, `F_CARRY`, `F_OVERFLOW`, `F_NEGATIVE`, `F_INT_ENABLED`, `F_INT` with helpers `F_SET`, `F_CLEAR`, `F_CHECK`.

**Memory**
- `RAM_SIZE = 0xFFFFFF` — used as initial stack pointer value.
- `ROM_SIZE = 0xFFFF` — BIOS image capacity.
- Memory access goes through the bus: `bus_read` / `bus_write`. Devices register address ranges and implement `read`/`write` handlers.

**Fetch / Execute**
- `fetch(machine)` macro reads `rom[pc++]` when in `BIOS` mode, otherwise `bus_read(m, pc++)`.
- `step()` increments `cpu.cycle`, checks for interrupts, then fetches a 32-bit instruction word and dispatches using `ops[opcode]`.

**Devices and Bus**
- Device interface in `emu/device.h` with `read`, `write`, `init`, `base`, `size`, `state`.
- `bus_register` keeps devices in an in-memory manager and calls `dev->init` on register.
- If no device handles an address, the bus falls back to `ram_read`/`ram_write` (see `emu/device.c`).

**Interrupts & BIOS interaction**
- On every 1000 cycles `step()` sets `F_INT` and `cpu.interrupt` is set to 0.
- When interrupts are enabled and present, the emulator synthesizes an `INT` operation. In kernel mode `INT` pushes PC and switches to BIOS handler addresses, in BIOS mode `IRET` restores mode and PC.
