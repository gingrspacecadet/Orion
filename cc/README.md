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

## Adding New Features

To add a new feature to the compiler, follow these steps:

### 1. Identify what you're adding
Determine if it's:
- **A keyword** (e.g., `while`, `if`, `asm`) → Token + Parser + Code generator
- **An operator** (e.g., `+`, `==`, `&&`) → Token + Parser (expression precedence) + Code generator
- **A statement type** (e.g., `return`, loops) → Token + Parser + Code generator
- **An expression type** (e.g., array access, struct member) → Parser + Code generator

### 2. Add token support (if needed)

In the `TokenKind` enum, add a new token type:
```c
typedef enum {
    TK_IDENT, TK_NUM, TK_KW_INT, TK_KW_RETURN, TK_KW_IF, TK_KW_ELSE,
    TK_KW_WHILE, TK_KW_FOR, TK_KW_ASM,  /* ← add here */
    /* ... other tokens ... */
    TK_EOF
} TokenKind;
```

In `next_token()`, add recognition for the new token:
- **Keywords**: Add a `strcmp()` check in the identifier parsing section:
  ```c
  else if (strcmp(s, "newkeyword") == 0) curtok.kind = TK_KW_NEWKEYWORD;
  ```
- **Symbols**: Add a `case` in the character switch statement:
  ```c
  case '?': curtok.kind = TK_QUESTION; break;
  ```
- **Strings/Literals**: Add custom parsing logic (see string parsing in the tokenizer)

### 3. Add AST node type (if needed)

In the `NodeKind` enum, add a new node kind:
```c
typedef enum {
    ND_NUM, ND_VAR, ND_CALL,
    ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD,
    ND_ASM,  /* ← add here */
    /* ... other nodes ... */
} NodeKind;
```

### 4. Update the parser

Determine where your feature fits in the parsing hierarchy:

**For statements** (if/while/return/asm):
- Add parsing in `parse_stmt()`:
  ```c
  if (curtok.kind == TK_KW_NEWFEATURE) {
      next_token();
      /* parse the feature */
      Node *n = new_stmt(ND_NEWFEATURE);
      n->/* fields */ = /* ... */;
      return n;
  }
  ```

**For expressions** (operators, function calls):
- Determine precedence level:
  - **Lowest**: Logical OR (`||`) → `parse_or()`
  - **Middle**: Logical AND (`&&`), comparison (`<`, `==`), arithmetic (`+`, `-`, `*`, `/`)
  - **Highest**: Primary (numbers, variables, parentheses) → `parse_primary()`
- Add a new function or insert into existing precedence level:
  ```c
  Node *parse_newop() {
      Node *n = parse_nextlevel();  /* parse higher precedence first */
      while (curtok.kind == TK_NEWOP) {
          next_token();
          n = new_bin(ND_NEWOP, n, parse_nextlevel());
      }
      return n;
  }
  ```
- Update the chain: `parse_or()` calls `parse_and()`, which calls `parse_cmp()`, etc.

### 5. Add code generation

In `gen_expr()` (for expressions) or `gen_stmt()` (for statements), emit assembly code:

```c
if (n->kind == ND_NEWFEATURE) {
    gen_expr(n->lhs);  /* generate left side */
    emit("    mov r1, r0\n");  /* save result */
    gen_expr(n->rhs);  /* generate right side */
    emit("    mov r2, r0\n");  /* save result */
    emit("    mov r0, r1\n");  /* restore left */
    emit("    newop r0, r0, r2\n");  /* emit actual instruction */
    return;
}
```

### 6. Test your feature

Create a test C file:
```c
int main() {
    /* test your feature here */
    return 0;
}
```

Compile and check the generated assembly:
```bash
./cc/cc test_feature.c output.s
cat output.s
```

Then assemble and run:
```bash
./asm/asm output.s output.out
./emu/emu output.out
```

### Example: Adding the `<<` (left shift) operator

**Step 1**: Add token
```c
typedef enum {
    /* ... */
    TK_LSHIFT,  /* ← add */
    /* ... */
};
```

In `next_token()`:
```c
case '<':
    if (src[pos] == '<') { pos++; curtok.kind = TK_LSHIFT; }  /* ← add */
    else if (src[pos] == '=') { pos++; curtok.kind = TK_LE; }
    else curtok.kind = TK_LT;
    break;
```

**Step 2**: Add node type
```c
typedef enum {
    /* ... */
    ND_LSHIFT,  /* ← add */
    /* ... */
};
```

**Step 3**: Add parsing (in `parse_add()` or a new precedence level)
```c
Node *parse_shift() {
    Node *n = parse_add();
    while (curtok.kind == TK_LSHIFT) {
        next_token();
        n = new_bin(ND_LSHIFT, n, parse_add());
    }
    return n;
}
```
Then update `parse_add()` to call `parse_shift()` instead of `parse_mul()`.

**Step 4**: Add code generation
```c
if (n->kind == ND_LSHIFT) {
    gen_expr(n->lhs);
    emit("    mov r1, r0\n");
    gen_expr(n->rhs);
    emit("    mov r2, r0\n");
    emit("    mov r0, r1\n");
    emit("    shl r0, r0, r2\n");  /* or whatever your ISA uses */
    return;
}
```

### Useful reference

- **Calling convention**: Arguments in r0-r3, return value in r0
- **Stack management**: r15 is the stack pointer; locals are indexed from it
- **Label generation**: Use `gen_label()` for unique labels
- **Code emission**: Use `emit(fmt, ...)` to write assembly
