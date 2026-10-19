#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

static inline Instr peek(Source *source) {
    size_t old = source->offset;
    Instr i = parse(source);
    source->offset = old;
    return i;
}

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

void codegen(FILE *out, Source *source) {
    Codegen builder = {
        .header = {
            .magic = 0x4F52004E,
            .version = 0x01000100,
            .sections = 1,
        },
        .active_section = 1,
        .sections[0] = {
            .name = 0,
            .data = 0,
            .align = 1,
            .size = 0,
            .id = 1,
        },
        .string_pool = ".text",
        .string_pool_offset = sizeof(".text"),
    };

    (void)out;
    (void)source;

    // TODO: parse instructions/directives, emit bytes, then serialise
    // the completed object.
}