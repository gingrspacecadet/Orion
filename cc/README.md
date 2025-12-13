Minimal C -> Orion assembly compiler

This is a tiny, educational C compiler that emits assembly compatible with the assembly files in this repository (see `test/`, `kernel/`, `bios/`).

Supported subset
- Function definitions: `int name() { ... }`
- `return` statements returning integer expressions
- Integer literals (decimal and 0x hex)
- Binary operators: `+`, `-`, `*`
- Function calls with no arguments: `foo()`

Code generation
- Functions are emitted as labels: `name:`
- Return value is placed in `r0`
- Calls are emitted as `call $name`
- `ret` is used to return from functions

Build
```
gcc -O2 -o cc/cc cc/cc.c
```

Usage
```
./cc/cc path/to/input.c path/to/output.s
```

Example
```
cat > example.c <<'C'
int put42() { return 42; }
int main() { return put42() + 1; }
C
./cc/cc example.c example.s
cat example.s
```

Notes
- This is intentionally tiny. It is a starting point you can extend to support local variables, control flow, more operators, and calling conventions.
