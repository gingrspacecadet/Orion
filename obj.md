# Internal object format

## Relocatable Objects

```c
typedef uint32_t section_id_t; // 0 == undefined
typedef uint32_t symbol_id_t;

typedef enum {
    RELOC_ABS32 = 1,
    RELOC_PCREL26,
    RELOC_PCREL13,
    RELOC_HI16,
    RELOC_LO16,
} reloc_type_t;

typedef struct {
    uint32_t magic; // "0R\0N"
    uint32_t version; // maj8 min8 isa8 pad8
    uint32_t sections;
    uint32_t syms;
    uint32_t relocations;
} obj_header_t;

typedef struct {
    char *name;
    uint8_t *data;
    uint32_t align;
    uint32_t size;
    uint8_t flags; // RWX, uninit
    section_id_t id;
} obj_section_t;

typedef struct {
    char *name;
    uint32_t offset;
    section_id_t section;
    uint8_t flags; // global
} obj_symbol_t;

typedef struct {
    uint32_t offset;
    section_id_t section;
    symbol_id_t symbol;
    int32_t addend;
    reloc_type_t type;
} obj_reloc_t;
```

## Relocation Semantics

`S` is the resolved symbol address, `A` is the signed addend, and `P` is the address of the four-byte instruction or word being patched. Relocation fields are encoded in little-endian words.

| Type | Calculation and patch |
|---|---|
| `ABS32` | Write `S + A` as the full 32-bit word. |
| `PCREL26` | Compute `(S + A - (P + 4)) / 4`; require word alignment and a signed 26-bit result; patch bits 25..0. |
| `PCREL13` | Compute `(S + A - (P + 4)) / 4`; require word alignment and a signed 13-bit result; patch bits 13..1, preserving the other bits. |
| `HI16` | Patch instruction immediate bits 17..2 with `((S + A + 0x8000) >> 16) & 0xffff`. This uses the signed-low-half convention for a `LUI` plus a sign-extending immediate operation. |
| `LO16` | Patch instruction immediate bits 17..2 with `(S + A) & 0xffff`. |

All calculations must be range-checked by the linker. `PCREL26` and `PCREL13` displacements are word counts, not byte counts. The `HI16` rounding adjustment is paired with `LO16` when the low half is consumed by a sign-extending immediate instruction such as `ADD`.

## Executable Images

```c
typedef uint32_t addr_t;

typedef struct {
    uint32_t magic; // "ORNE"
    uint32_t version; // maj8 min8 isa8 pad8
    addr_t entry;
    uint32_t sections;
} exe_header_t;

typedef struct {
    addr_t address;
    uint32_t offset;
    uint32_t memory_size;
    uint32_t file_size; // file_size <= memory_size
    uint32_t align;
    uint8_t flags; // RWX
} exe_section_t;
```