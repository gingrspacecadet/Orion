# Contributing

Quick checklist for changes:

- Update code under `emu/`, `asm/`, `bios/`, or `kernel/` as needed.
- Add new opcodes in `asm/ops.h` and implement assembler encoding in `asm/main.c` when necessary.
- Implement the runtime handler in `emu/ops.h` and add it to the `ops[]` dispatch table in `emu/main.c`.
- Add or update device drivers in `emu/devices/` and register them with `bus_register`.
- Add regression tests under `test/` and verify by assembling and running the emulator.
- Update documentation in `docs_full/` (and `docs/` as appropriate).

Build & test locally:

```
make asm
make bios
make kernel
make
./build/orion kernel.out bios.out
```

PR checklist:
- Purpose and summary
- Test plan and sample invocation
- Any backwards-incompatible changes documented
- Docs updated
