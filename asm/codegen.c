#include <stdbool.h>
#include <strings.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "codegen.h"
#include "parser.h"
#include "obj.h"
#include "isa.h"

#define MAX_ENTRIES UINT8_MAX
#define MAX_SECTION_SIZE UINT8_MAX

typedef struct {
    obj_header_t header;
    obj_section_t sections[MAX_ENTRIES];
    obj_symbol_t symbols[MAX_ENTRIES];
    obj_reloc_t relocs[MAX_ENTRIES];

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
    section->data = 0;
    section->align = strcmp(name, ".text") == 0 ? 4 : 1;
    section->id = (section_id_t)(index + 1);
    // provide some defaults
    if (strcmp(name, ".text") == 0)
        section->flags = FLAG_R | FLAG_X;
    else if (strcmp(name, ".rodata") == 0)
        section->flags = FLAG_R;
    else if (strcmp(name, ".data") == 0)
        section->flags = FLAG_R | FLAG_W;
    else if (strcmp(name, ".bss") == 0)
        section->flags = FLAG_R | FLAG_W | FLAG_U;
    else
        section->flags = FLAG_R | FLAG_X;

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
    for (size_t i = 0; i < builder->header.symbols; i++) {
        obj_symbol_t *sym = &builder->symbols[i];

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

    if (builder->header.symbols >= MAX_ENTRIES)
        return false;

    stroff_t name_offset;
    if (!append_string(builder, name, &name_offset))
        return false;

    size_t index = builder->header.symbols;
    obj_symbol_t *sym = &builder->symbols[index];
    memset(sym, 0, sizeof(*sym));

    sym->name = name_offset;
    sym->section = 0; // undefined until defined by a label.

    *id = (symbol_id_t)index;
    builder->header.symbols++;

    return true;
}

static bool define_symbol(Codegen *builder, const char *name) {
    obj_section_t *section = active_section(builder);
    if (section == NULL)
        return false;

    symbol_id_t id;
    if (!get_or_create_symbol(builder, name, &id))
        return false;

    obj_symbol_t *sym = &builder->symbols[id];

    if (sym->section != 0)
        return false; // Duplicate definition.

    sym->section = section->id;
    sym->offset = section->size;

    return true;
}

static bool add_relocation(Codegen *builder, uint32_t offset, reloc_type_t type, symbol_id_t symbol, int32_t addend) {
    obj_section_t *section = active_section(builder);
    if (section == NULL || symbol >= builder->header.symbols ||
        builder->header.relocs >= MAX_ENTRIES)
        return false;

    // all current relocation types patch a four-byte word.
    if (offset > section->size || section->size - offset < sizeof(uint32_t))
        return false;

    obj_reloc_t *reloc = &builder->relocs[builder->header.relocs++];

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
        "%s expects %u operand%s, got %u", (I)->name, \
        (unsigned)(N), (N) == 1 ? "" : "s", (I)->op_count)

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

    builder->symbols[id].flags |= SYMFLAG_GLOBAL;

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

static uint32_t encode_x(Opcodes code) {
    uint32_t word = 0;
    word |= (code & 0x7F) << 26;
    return word;
}

static uint32_t encode_a(Opcodes code, uint8_t rn, uint8_t rd, uint16_t operand, bool reg) {
    uint32_t word = 0;
    word |= (code & 0x7F) << 26;
    word |= (rn & 0xF) << 22;
    word |= (rd & 0xF) << 18;
    if (reg) word |= (operand &    0xF) << 2;
    else     word |= (operand & 0xFFFF) << 2;
    word |= (reg & 0x1) << 1;
    return word;
}

static uint32_t encode_m(Opcodes code, uint8_t rn, uint8_t rd, uint16_t operand, bool reg) {
    return encode_a(code, rn, rd, operand, reg);
}

static uint32_t encode_j(Opcodes code, uint32_t displacement) {
    uint32_t word = 0;
    word |= (code & 0x7F) << 26;
    word |= (displacement & 0x3FFFFFF);
    return word;
}

typedef struct {
    const char *name;
    OpcodeTypes type;
    Opcodes code;
} OpConfig;

const OpConfig opcodes[] = {
    {"nop", TYPE_X, OP_NOP},
    {"add", TYPE_A, OP_ADD},
    {"sub", TYPE_A, OP_SUB},
    {"mul", TYPE_A, OP_MUL},
    {"div", TYPE_A, OP_DIV},
    {"divu", TYPE_A, OP_DIVU},
    {"lui", TYPE_M, OP_LUI},
    {"ldr", TYPE_M, OP_LDR},
    {"jmp", TYPE_J, OP_JMP},
    {"call", TYPE_J, OP_CALL},
};

static bool emit_reloc(Codegen *builder, symbol_id_t symbol, reloc_type_t type, dataoff_t offset) {
    obj_reloc_t *reloc = &builder->relocs[builder->header.relocs++];

    reloc->type = type;
    reloc->section = active_section(builder)->id;
    reloc->offset = offset;
    reloc->symbol = symbol;

    return true;
}

static bool emit_instr(Codegen *builder, const Instr *i) {
    OpConfig conf = {0};
    for (size_t j = 0; j < sizeof(opcodes) / sizeof(opcodes[0]); j++) {
        if (strcasecmp(i->name, opcodes[j].name) == 0) {
            conf = opcodes[j];
        }
    }

    if (conf.type == 0) {
        asm_error(i->line_num, "Unknown instruction type %s", i->name);
        return false;
    }

    // handle special cases like LUI

    if (conf.code == OP_LUI) {
        REQUIRE_OP_COUNT(i, 2);
        REQUIRE_OP_MODE(i, 0, AM_REG, "a register");
        REQUIRE_OP_MODE(i, 1, AM_IMM, "an immediate");

        emit_word(builder, encode_a(OP_LUI, 0, i->ops[0].reg, i->ops[1].val, false));

        return true;
    }

    switch (conf.type) {
        case TYPE_X: {
            REQUIRE_OP_COUNT(i, 0);
            emit_word(builder, encode_x(conf.code));
            break;
        }

        case TYPE_A:
        case TYPE_M: {
            REQUIRE_OP_COUNT(i, 3);
            REQUIRE_OP_MODE(i, 0, AM_REG, "a register"); REQUIRE_OP_MODE(i, 1, AM_REG, "a register");
            if (i->ops[2].mode == AM_REG) {
                emit_word(builder, encode_a(conf.code, i->ops[1].reg, i->ops[0].reg, i->ops[2].reg, true));
            } else if (i->ops[2].mode == AM_IMM) {
                emit_word(builder, encode_a(conf.code, i->ops[1].reg, i->ops[0].reg, i->ops[2].val, false));
            } else {
                asm_error(i->line_num, "%s operand 3 must be either a register or an immediate\n", i->name);
                return false;
            }

            break;
        }

        case TYPE_J: {
            REQUIRE_OP_COUNT(i, 1);
            if (i->ops[0].mode == AM_IMM) {
                REQUIRE(i, (uint32_t)i->ops[0].val < 0x3FFFFFF, "%s argument must fit in 26 bits\n", i->name);
                emit_word(builder, encode_j(conf.code, i->ops[0].val));
            } else if (i->ops[0].mode == AM_REF) {
                symbol_id_t id;
                get_or_create_symbol(builder, i->ops[0].label, &id);
                emit_word(builder, encode_j(conf.code, 0));
                emit_reloc(builder, id, RELOC_PCREL26, active_section(builder)->size - sizeof(uint32_t));
            } else {

            }

            break;
        }

        default: {
            asm_error(i->line_num, "unimplemented instruction %s", i->name);
            return false;
        }
    }

    return true;
}

void codegen(FILE *out, Source *source) {
    Codegen builder = {
        .header = {
            .magic = OBJ_MAGIC,
            .version = 0x01000100,
        },
    };

    switch_section(&builder, ".text");

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
        } else {
            emit_instr(&builder, &i);
        }
    }

    uint32_t data_offset = 0;
    for (uint32_t i = 0; i < builder.header.sections; i++) {
        builder.sections[i].data = data_offset;
        data_offset += builder.sections[i].size;
        builder.header.data_pool_size += builder.sections[i].size;
    }

    builder.header.string_pool_size = builder.string_pool_offset;

    fwrite(&builder.header, sizeof(obj_header_t), 1, out);
    fwrite(&builder.sections[0], sizeof(obj_section_t), builder.header.sections, out);
    fwrite(&builder.symbols[0], sizeof(obj_symbol_t), builder.header.symbols, out);
    fwrite(&builder.relocs[0], sizeof(obj_reloc_t), builder.header.relocs, out);

    for (uint32_t i = 0; i < builder.header.sections; i++) {
        fwrite(builder.data_pool[i], sizeof(uint8_t), builder.sections[i].size, out);
    }

    fwrite(builder.string_pool, sizeof(uint8_t), builder.header.string_pool_size, out);
}