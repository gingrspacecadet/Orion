# Tests & Examples

Sample programs are in `test/` and include small demos and tests assembled with the project's assembler.

Examples:
- `test/test.s` — simple program that prints a string using `ldr`/`str` and `call` to a `printchar` routine.
- `test/loop.s`, `test/push.s`, `test/fibonacci.s` — small assembly tests covering looping, stack ops, and arithmetic.

Running tests
- Assemble tests using `./build/asm` then run with the emulator: `./build/orion test_program.out bios.out`.
