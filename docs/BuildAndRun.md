# Build & Run

Requirements
- `gcc`, `make`, `pkg-config` and SDL2 development packages (used when linking the emulator).

Common commands (run from project root):

```
make           # builds assembler and emulator; also generates kernel.out and bios.out via targets
make asm       # builds the assembler at build/asm
make bios      # runs assembler on bios/main.s -> bios.out
make kernel    # runs assembler on kernel/main.s -> kernel.out
make run       # builds and runs: ./build/orion kernel.out bios.out
```

Assembling a single file:

```
./build/asm path/to/file.s out.bin
```

Running the emulator directly:

```
./build/orion <program.out> [bios.out]
```

Notes
- The `Makefile` compiles C sources under `emu/` and places objects in `build/`.
- The assembler is a small single-file tool in `asm/main.c`.
