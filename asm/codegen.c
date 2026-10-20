#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "codegen.h"
#include "parser.h"
#include "obj.h"

#define MAX_ENTRIES UINT8_MAX
#define MAX_SECTION_SIZE UINT8_MAX

typedef struct {
    obj_header_t header;
    obj_section_t sections[MAX_ENTRIES];
    obj_symbol_t syms[MAX_ENTRIES];
    obj_reloc_t relocations[MAX_ENTRIES];

    char string_pool[MAX_ENTRIES];
    size_t string_pool_offset;
    uint8_t data_pool[MAX_ENTRIES][MAX_SECTION_SIZE];

    section_id_t active_section;
} Codegen;

static inline obj_section_t *active_section(Codegen *builder) {
    if (builder->active_section == 0 ||
        builder->active_section > builder->header.sections)
        return NULL;

    return &builder->sections[builder->active_section - 1];
}

static bool append_string(Codegen *builder, const char *name, stroff_t *offset) {
    size_t len = strlen(name) + 1;

    if (builder->string_pool_offset > sizeof(builder->string_pool) || len > sizeof(builder->string_pool) - builder->string_pool_offset)
        return false;

    *offset = (stroff_t)builder->string_pool_offset;
    memcpy(&builder->string_pool[builder->string_pool_offset], name, len);
    builder->string_pool_offset += len;

    return true;
}

static bool switch_section(Codegen *builder, const char *name) {
    for (size_t i = 0; i < builder->header.sections; i++) {
        obj_section_t *section = &builder->sections[i];

        if (section->name >= builder->string_pool_offset)
            return false;

        if (strcmp(&builder->string_pool[section->name], name) == 0) {
            builder->active_section = section->id;
            return true;
        }
    }

    if (builder->header.sections >= MAX_ENTRIES)
        return false;

    stroff_t name_offset;
    if (!append_string(builder, name, &name_offset))
        return false;

    size_t index = builder->header.sections;
    obj_section_t *section = &builder->sections[index];
    memset(section, 0, sizeof(*section));

    section->name = name_offset;
    section->data = (dataoff_t)(index * sizeof(builder->data_pool[0]));
    section->align = 1;
    section->id = (section_id_t)(index + 1);

    builder->header.sections++;
    builder->active_section = section->id;

    return true;
}

static bool emit_byte(Codegen *builder, uint8_t byte) {
    obj_section_t *section = active_section(builder);
    if (section == NULL || section->size >= MAX_SECTION_SIZE)
        return false;

    builder->data_pool[section->id - 1][section->size] = byte;
    section->size++;

    return true;
}

static bool emit_word(Codegen *builder, uint32_t word) {
    obj_section_t *section = active_section(builder);
    if (section == NULL || section->size > MAX_SECTION_SIZE || MAX_SECTION_SIZE - section->size < sizeof(word))
        return false;

    size_t index = section->id - 1;
    size_t offset = section->size;

    builder->data_pool[index][offset]     = (uint8_t)word;
    builder->data_pool[index][offset + 1] = (uint8_t)(word >> 8);
    builder->data_pool[index][offset + 2] = (uint8_t)(word >> 16);
    builder->data_pool[index][offset + 3] = (uint8_t)(word >> 24);

    section->size += sizeof(word);
    return true;
}

static bool find_symbol(Codegen *builder, const char *name, symbol_id_t *id) {
    for (size_t i = 0; i < builder->header.syms; i++) {
        obj_symbol_t *sym = &builder->syms[i];

        if (sym->name >= builder->string_pool_offset)
            return false;

        if (strcmp(&builder->string_pool[sym->name], name) == 0) {
            *id = (symbol_id_t)i;
            return true;
        }
    }

    return false;
}

static bool get_or_create_symbol(Codegen *builder, const char *name, symbol_id_t *id) {
    if (find_symbol(builder, name, id))
        return true;

    if (builder->header.syms >= MAX_ENTRIES)
        return false;

    stroff_t name_offset;
    if (!append_string(builder, name, &name_offset))
        return false;

    size_t index = builder->header.syms;
    obj_symbol_t *sym = &builder->syms[index];
    memset(sym, 0, sizeof(*sym));

    sym->name = name_offset;
    sym->section = 0; // undefined until defined by a label.

    *id = (symbol_id_t)index;
    builder->header.syms++;

    return true;
}

static bool define_symbol(Codegen *builder, const char *name) {
    obj_section_t *section = active_section(builder);
    if (section == NULL)
        return false;

    symbol_id_t id;
    if (!get_or_create_symbol(builder, name, &id))
        return false;

    obj_symbol_t *sym = &builder->syms[id];

    if (sym->section != 0)
        return false; // Duplicate definition.

    sym->section = section->id;
    sym->offset = section->size;

    return true;
}

static bool add_relocation(Codegen *builder, uint32_t offset, reloc_type_t type, symbol_id_t symbol, int32_t addend) {
    obj_section_t *section = active_section(builder);
    if (section == NULL || symbol >= builder->header.syms ||
        builder->header.relocations >= MAX_ENTRIES)
        return false;

    // all current relocation types patch a four-byte word.
    if (offset > section->size || section->size - offset < sizeof(uint32_t))
        return false;

    obj_reloc_t *reloc = &builder->relocations[builder->header.relocations++];

    *reloc = (obj_reloc_t){
        .section = section->id,
        .offset = offset,
        .symbol = symbol,
        .addend = addend,
        .type = type,
    };

    return true;
}

static void asm_error(int line, const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "Assembler Error (line %d): ", line);

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
}

#define REQUIRE(I, COND, ...) \
    do { \
        if (!(COND)) { \
            asm_error((I)->line_num, __VA_ARGS__); \
            return false; \
        } \
    } while (0)

#define REQUIRE_OP_COUNT(I, N) \
    REQUIRE((I), (I)->op_count == (N), \
        "%s expects %u operand%s", (I)->name, \
        (unsigned)(N), (N) == 1 ? "" : "s")

#define REQUIRE_OP_MODE(I, INDEX, MODE, DESC) \
    REQUIRE((I), (I)->ops[(INDEX)].mode == (MODE), \
        "%s operand %zu must be %s", (I)->name, \
        (size_t)(INDEX) + 1, (DESC))

static bool handle_align(Codegen *builder, const Instr *i) {
    REQUIRE_OP_COUNT(i, 1);
    REQUIRE_OP_MODE(i, 0, AM_IMM, "an immediate");

    int64_t requested = i->ops[0].val;
    REQUIRE(i, requested > 0, ".align must be positive");

    obj_section_t *section = active_section(builder);
    REQUIRE(i, section != NULL, "no active section");
    REQUIRE(i, section->size <= MAX_SECTION_SIZE, "section size exceeds capacity");

    size_t alignment = (size_t)requested;
    size_t padding = (alignment - section->size % alignment) % alignment;

    REQUIRE(i, padding <= MAX_SECTION_SIZE - section->size, ".align exceeds section capacity");

    for (size_t n = 0; n < padding; n++)
        REQUIRE(i, emit_byte(builder, 0), "failed to emit alignment padding");

    return true;
}

static bool handle_section(Codegen *builder, const Instr *i) {
    REQUIRE_OP_COUNT(i, 1);
    REQUIRE_OP_MODE(i, 0, AM_REF, "a section");

    bool ok = switch_section(builder, i->ops[0].label);
    REQUIRE(i, ok, "failed to switch to section %s", i->ops[0].label);

    return ok;
}

static bool handle_byte(Codegen *builder, const Instr *i) {
    REQUIRE(i, i->op_count > 0, "missing argument to .byte");
    for (uint8_t j = 0; j < i->op_count; j++) {
        REQUIRE_OP_MODE(i, j, AM_IMM, "an immediate");
        REQUIRE(i, i->ops[j].val >= -UINT8_MAX && i->ops[j].val <= UINT8_MAX, "argument must be within the byte range");
    }

    bool ok = true;
    for (uint8_t j = 0; j < i->op_count; j++) {
        bool r = emit_byte(builder, (uint8_t)i->ops[j].val);
        REQUIRE(i, r, "Failed to emit byte");
        if (!r) ok = false;
    }

    return ok;
}

static bool handle_word(Codegen *builder, const Instr *i) {
    REQUIRE(i, i->op_count > 0, "missing argument to .word");
    for (uint8_t j = 0; j < i->op_count; j++) {
        REQUIRE_OP_MODE(i, j, AM_IMM, "an immediate");
        REQUIRE(i, i->ops[j].val >= -UINT32_MAX && i->ops[j].val <= UINT32_MAX, "argument must be within the word range");
    }

    bool ok = true;
    for (uint8_t j = 0; j < i->op_count; j++) {
        bool r = emit_word(builder, (uint32_t)i->ops[j].val);
        REQUIRE(i, r, "Failed to emit word");
        if (!r) ok = false;
    }

    return ok;
}

static bool handle_global(Codegen *builder, const Instr *i) {
    REQUIRE_OP_COUNT(i, 1);
    REQUIRE_OP_MODE(i, 0, AM_REF, "a symbol");

    symbol_id_t id;
    REQUIRE(i, get_or_create_symbol(builder, i->ops[0].label, &id), "Failed to create symbol %s", i->ops[0].label);

    builder->syms[id].flags |= SYMFLAG_GLOBAL;

    return true;
}

struct {
    const char *const name;
    bool (*fn)(Codegen *, const Instr *);
} directives[] = {
    {".align", handle_align},
    {".section", handle_section},
    {".byte", handle_byte},
    {".word", handle_word},
    {".global", handle_global},
};

void codegen(FILE *out, Source *source) {
    Codegen builder = {
        .header = {
            .magic = 0x4F52004E,
            .version = 0x01000100,
        },
    };

    switch_section(&builder, ".text");

    (void)out;
    (void)source;

    bool ok = true;
    for (Instr i = parse(source); i.is_eof != true; i = parse(source)) {
        if (i.is_directive) {
            bool found = false;

            for (size_t j = 0; j < sizeof(directives) / sizeof(directives[0]); j++) {
                if (strcmp(i.name, directives[j].name) == 0) {
                    ok = directives[j].fn(&builder, &i) ? ok : false;
                    found = true;
                }
            }

            if (!found) {
                asm_error(i.line_num, "Unknown directive %s", i.name);
                ok = false;
            }
        } else if (i.is_label) {
            if (!define_symbol(&builder, i.name))
                fprintf(stderr, "Assembler Error (line %d): Failed to create symbol %s\n", i.line_num, i.name);
        }
    }
}