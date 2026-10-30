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

#define MAX_ENTRIES 1024
#define MAX_SECTION_SIZE 1024

typedef struct {
    obj_header_t header;
    obj_section_t sections[MAX_ENTRIES];
    obj_symbol_t symbols[MAX_ENTRIES];
    obj_reloc_t relocs[MAX_ENTRIES];

    char string_pool[MAX_ENTRIES];
    size_t string_pool_offset;
    uint8_t data_pool[MAX_ENTRIES][MAX_SECTION_SIZE];

    section_id_t active_section;

    bool failed;
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
            builder->failed = true; \
            return false; \
        } \
    } while (0)

#define REQUIRE_OP_COUNT(I, N) \
    REQUIRE((I), (I)->op_count == (N), \
        "%s expects %u operand%s, got %u", (I)->name, \
        (unsigned)(N), (N) == 1 ? "" : "s", (I)->op_count)

static const char *const op_mode_desc[] = {
    [AM_IMM] = "an immediate",
    [AM_MEM] = "a memory address",
    [AM_NONE] = "<none>",
    [AM_REF] = "a reference",
    [AM_REG] = "a register",
    [AM_REGLIST] = "a register list"
};

#define REQUIRE_OP_MODE(I, INDEX, MODE) \
    REQUIRE((I), (I)->ops[(INDEX)].mode == (MODE), \
        "%s operand %zu must be %s", (I)->name, \
        (size_t)(INDEX) + 1, op_mode_desc[MODE])

static bool handle_align(Codegen *builder, const Instr *i) {
    REQUIRE_OP_COUNT(i, 1);
    REQUIRE_OP_MODE(i, 0, AM_IMM);

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
    REQUIRE_OP_MODE(i, 0, AM_IDENT);

    bool ok = switch_section(builder, i->ops[0].label);
    REQUIRE(i, ok, "failed to switch to section %s", i->ops[0].label);

    return ok;
}

static bool handle_byte(Codegen *builder, const Instr *i) {
    REQUIRE(i, i->op_count > 0, "missing argument to .byte");
    for (uint8_t j = 0; j < i->op_count; j++) {
        REQUIRE_OP_MODE(i, j, AM_IMM);
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

static bool emit_reloc(Codegen *builder, symbol_id_t symbol, reloc_type_t type, dataoff_t offset);

static bool handle_word(Codegen *builder, const Instr *i) {
    REQUIRE(i, i->op_count > 0, "missing argument to .word");
    for (uint8_t j = 0; j < i->op_count; j++) {
        if (i->ops[j].mode != AM_IMM && i->ops[j].mode != AM_REF) {
            asm_error(i->line_num, ".word operand 1 must be an immediate or a reference");
            builder->failed = true;
            return false;
        }
        if (i->ops[j].mode == AM_IMM) REQUIRE(i, (i->ops[j].val & UINT32_MAX) == i->ops[j].val, ".word argument must be within the word range");
    }

    for (uint8_t j = 0; j < i->op_count; j++) {
        if (i->ops[j].mode == AM_IMM) {
            REQUIRE(i, emit_word(builder, (uint32_t)i->ops[j].val), "Failed to emit word");
        } else {
            REQUIRE(i, emit_word(builder, 0), "Failed to emit word");
            symbol_id_t id;
            REQUIRE(i, get_or_create_symbol(builder, i->ops[j].label, &id), "Failed to find symbol \"%s\"", i->ops[j].label);
            REQUIRE(i, emit_reloc(builder, id, RELOC_ABS32, active_section(builder)->size - sizeof(uint32_t)), "Failed to emit relocation\n");
        }
    }

    return true;
}

static bool handle_global(Codegen *builder, const Instr *i) {
    REQUIRE_OP_COUNT(i, 1);
    REQUIRE_OP_MODE(i, 0, AM_IDENT);

    symbol_id_t id;
    REQUIRE(i, get_or_create_symbol(builder, i->ops[0].label, &id), "Failed to create symbol %s", i->ops[0].label);

    builder->symbols[id].flags |= SYMFLAG_GLOBAL;

    return true;
}

static bool handle_zero(Codegen *builder, const Instr *i) {
    REQUIRE_OP_COUNT(i, 1);
    REQUIRE_OP_MODE(i, 0, AM_IMM);

    REQUIRE(i, i->ops[0].val > 0, ".zero operand must be greater than 0");

    for (size_t j = 0; j < (size_t)i->ops[0].val; j++) {
        emit_byte(builder, 0);
    }

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
    {".zero", handle_zero},
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

static uint32_t encode_b(Opcodes code, uint8_t cond, uint8_t rn, uint8_t operand, int16_t displacement, bool reg) {
    uint32_t word = 0;
    word |= (code & 0x7F) << 26;
    word |= (cond & 0x7) << 23;
    word |= (rn & 0xF) << 19;
    word |= (operand & 0x1F) << 14;
    word |= (displacement & 0x1FFF) << 1;
    word |= (reg & 0x1) << 0;
    return word;
}

static uint8_t branch_condition(const char *name) {
    if (strcasecmp(name, "jeq") == 0) return 1;
    if (strcasecmp(name, "jne") == 0) return 2;
    if (strcasecmp(name, "jlt") == 0) return 3;
    if (strcasecmp(name, "jge") == 0) return 4;
    if (strcasecmp(name, "jltu") == 0) return 5;
    if (strcasecmp(name, "jgeu") == 0) return 6;
    return 0;
}

static uint32_t encode_s(Opcodes code, uint8_t rn, uint16_t payload, bool reg, bool write) {
    uint32_t word = 0;
    word |= (code & 0x7F) << 26;
    word |= (rn & 0xF) << 22;
    word |= (payload & 0xFFFF) << 6;
    word |= (reg & 0x1) << 1;
    word |= (write & 0x1) << 0;
    return word;
}

static uint32_t encode_i(Opcodes code, uint8_t rn) {
    uint32_t word = 0;
    word |= (code & 0x7F) << 26;
    word |= (rn & 0xF) << 22;
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
    {"shl", TYPE_A, OP_SHL},
    {"shr", TYPE_A, OP_SHR},
    {"ashr", TYPE_A, OP_ASHR},
    {"and", TYPE_A, OP_AND},
    {"or", TYPE_A, OP_OR},
    {"xor", TYPE_A, OP_XOR},
    {"lui", TYPE_A, OP_LUI},
    {"ldr", TYPE_M, OP_LDR},
    {"ldrb", TYPE_M, OP_LDRB},
    {"str", TYPE_M, OP_STR},
    {"strb", TYPE_M, OP_STRB},
    {"jmp", TYPE_J, OP_JMP},
    {"call", TYPE_J, OP_CALL},
    {"jmpr", TYPE_I, OP_JMPR},
    {"jeq", TYPE_B, OP_JXX},
    {"jne", TYPE_B, OP_JXX},
    {"jlt", TYPE_B, OP_JXX},
    {"jge", TYPE_B, OP_JXX},
    {"jltu", TYPE_B, OP_JXX},
    {"jgeu", TYPE_B, OP_JXX},
    {"iret", TYPE_X, OP_IRET},
    {"ret", TYPE_X, OP_RET},
    {"halt", TYPE_X, OP_HALT},
};

static bool emit_reloc(Codegen *builder, symbol_id_t symbol, reloc_type_t type, dataoff_t offset) {
    assert(builder->header.relocs < MAX_ENTRIES);
    obj_reloc_t *reloc = &builder->relocs[builder->header.relocs++];

    reloc->type = type;
    reloc->section = active_section(builder)->id;
    reloc->offset = offset;
    reloc->symbol = symbol;

    return true;
}

static bool emit_instr(Codegen *builder, const Instr *i) {
    // handle pseuoinstructions

    if (strcasecmp(i->name, "mov") == 0) {
        REQUIRE_OP_COUNT(i, 2);
        REQUIRE_OP_MODE(i, 0, AM_REG);

        if (i->ops[1].mode == AM_IMM) {
            emit_word(builder, encode_a(OP_XOR, i->ops[0].reg, i->ops[0].reg, i->ops[0].reg, true));
            if (i->ops[1].val > UINT16_MAX) {
                emit_word(builder, encode_a(OP_LUI, 0, i->ops[0].reg, i->ops[1].val >> 16, false));
            }

            emit_word(builder, encode_a(OP_ADD, i->ops[0].reg, i->ops[0].reg, i->ops[1].val, false));
        } else if (i->ops[1].mode == AM_REF) {
            symbol_id_t id;
            emit_word(builder, encode_a(OP_XOR, i->ops[0].reg, i->ops[0].reg, i->ops[0].reg, true));
            emit_word(builder, encode_a(OP_LUI, 0, i->ops[0].reg, 0, false));
            REQUIRE(i, get_or_create_symbol(builder, i->ops[1].label, &id), "failed to get id of symbol %s", i->ops[1].label);
            REQUIRE(i, emit_reloc(builder, id, RELOC_HI16, active_section(builder)->size - sizeof(uint32_t)), "failed to emit relocation");

            emit_word(builder, encode_a(OP_ADD, i->ops[0].reg, i->ops[0].reg, 0, false));
            REQUIRE(i, get_or_create_symbol(builder, i->ops[1].label, &id), "failed to get id of symbol %s", i->ops[1].label);
            REQUIRE(i, emit_reloc(builder, id, RELOC_LO16, active_section(builder)->size - sizeof(uint32_t)), "failed to emit relocation");
        } else {
            asm_error(i->line_num, "Unknown MOV format");
            return false;
        }

        return true;
    } else if (strcasecmp(i->name, "flagsw") == 0) {
        REQUIRE_OP_COUNT(i, 1);
        if (i->ops[0].mode == AM_IMM) {
            emit_word(builder, encode_s(OP_FLAGS, 0, i->ops[0].val, false, true));
        } else if (i->ops[0].mode == AM_REG) {
            emit_word(builder, encode_s(OP_FLAGS, i->ops[0].reg, 0, true, true));
        } else {
            REQUIRE_OP_MODE(i, 0, AM_REG);
        }

        return true;
    } else if (strcasecmp(i->name, "push") == 0) {
        REQUIRE_OP_COUNT(i, 1);
        if (i->ops[0].mode == AM_REG) {
            emit_word(builder, encode_a(OP_SUB, i->ops[0].reg, i->ops[0].reg, 4, false));
            emit_word(builder, encode_m(OP_STR, 0, i->ops[0].reg, 0, false));
        } else if (i->ops[0].mode == AM_REGLIST) {
            for (int j = 0; j < 16; j++) {
                if (i->ops[0].val & (1u << j)) {
                    emit_word(builder, encode_a(OP_SUB, j, j, 4, false));
                    emit_word(builder, encode_m(OP_STR, 0, j, 0, false));
                }
            }
        } else {
            REQUIRE_OP_MODE(i, 0, AM_REG);
        }

        return true;
    } else if (strcasecmp(i->name, "pop") == 0) {
        REQUIRE_OP_COUNT(i, 1);
        if (i->ops[0].mode == AM_REG) {
            emit_word(builder, encode_m(OP_LDR, 0, i->ops[0].reg, 0, false));
            emit_word(builder, encode_a(OP_ADD, i->ops[0].reg, i->ops[0].reg, 4, false));
        } else if (i->ops[0].mode == AM_REGLIST) {
            for (int j = 0; j < 16; j++) {
                if (i->ops[0].val & (1u << j)) {
                    emit_word(builder, encode_m(OP_LDR, 0, j, 0, false));
                    emit_word(builder, encode_a(OP_ADD, j, j, 4, false));
                }
            }
        } else {
            REQUIRE_OP_MODE(i, 0, AM_REG);
        }

        return true;
    }

    OpConfig conf = {0};
    for (size_t j = 0; j < sizeof(opcodes) / sizeof(opcodes[0]); j++) {
        if (strcasecmp(i->name, opcodes[j].name) == 0) {
            conf = opcodes[j];
        }
    }

    if (conf.type == 0) {
        asm_error(i->line_num, "Unknown instruction type %s", i->name);
        builder->failed = true;
        return false;
    }

    // handle special cases like LUI

    if (conf.code == OP_LUI) {
        REQUIRE_OP_COUNT(i, 2);
        REQUIRE_OP_MODE(i, 0, AM_REG);
        REQUIRE_OP_MODE(i, 1, AM_IMM);

        emit_word(builder, encode_a(OP_LUI, 0, i->ops[0].reg, i->ops[1].val, false));

        return true;
    }

    switch (conf.type) {
        case TYPE_X: {
            REQUIRE_OP_COUNT(i, 0);
            emit_word(builder, encode_x(conf.code));
            break;
        }

        case TYPE_A: {
            REQUIRE_OP_COUNT(i, 3);
            REQUIRE_OP_MODE(i, 0, AM_REG); REQUIRE_OP_MODE(i, 1, AM_REG);
            if (i->ops[2].mode == AM_REG) {
                emit_word(builder, encode_a(conf.code, i->ops[1].reg, i->ops[0].reg, i->ops[2].reg, true));
            } else if (i->ops[2].mode == AM_IMM) {
                emit_word(builder, encode_a(conf.code, i->ops[1].reg, i->ops[0].reg, i->ops[2].val, false));
            } else {
                asm_error(i->line_num, "%s operand 3 must be either a register or an immediate\n", i->name);
                builder->failed = true;
                return false;
            }

            break;
        }

        case TYPE_M: {
            REQUIRE_OP_COUNT(i, 2);
            REQUIRE_OP_MODE(i, 0, AM_REG);

            if (i->ops[1].mode == AM_MEM) {
                emit_word(builder, encode_m(conf.code, i->ops[1].reg, i->ops[0].reg, i->ops[1].val, false));
            } else if (i->ops[1].mode == AM_REG) {
                emit_word(builder, encode_m(conf.code, i->ops[1].reg, i->ops[0].reg, 0, false));
            } else {
                REQUIRE_OP_MODE(i, 1, AM_MEM);
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
                REQUIRE_OP_MODE(i, 0, AM_REF);
            }

            break;
        }

        case TYPE_B: {
            REQUIRE_OP_COUNT(i, 3);
            REQUIRE_OP_MODE(i, 0, AM_REG);
            uint8_t cond = branch_condition(i->name);
            if (cond == 0) {
                asm_error(i->line_num, "Unknown branch condition %s\n", i->name);
                builder->failed = true;
                return false;
            }

            if (i->ops[1].mode == AM_REG) {
                if (i->ops[1].reg < 0 || i->ops[1].reg > 15) {
                    asm_error(i->line_num, "%s second operand must be a register or immediate\n", i->name);
                    builder->failed = true;
                    return false;
                }
            } else if (i->ops[1].mode != AM_IMM) {
                REQUIRE_OP_MODE(i, 1, AM_REG);
            }

            uint8_t operand = 0;
            bool reg_rhs = false;
            if (i->ops[1].mode == AM_REG) {
                operand = (uint8_t)(i->ops[1].reg & 0xF);
                reg_rhs = true;
            } else if (i->ops[1].mode == AM_IMM) {
                operand = (uint8_t)(i->ops[1].val & 0x1F);
                reg_rhs = false;
            }

            if (i->ops[2].mode == AM_REF) {
                symbol_id_t id;
                get_or_create_symbol(builder, i->ops[2].label, &id);
                emit_word(builder, encode_b(OP_JXX, cond, i->ops[0].reg, operand, 0, reg_rhs));
                emit_reloc(builder, id, RELOC_PCREL13, active_section(builder)->size - sizeof(uint32_t));
            } else if (i->ops[2].mode == AM_IMM) {
                REQUIRE(i, (uint32_t)i->ops[2].val < 0x2000, "%s displacement must fit in 13 bits\n", i->name);
                emit_word(builder, encode_b(OP_JXX, cond, i->ops[0].reg, operand, (int16_t)i->ops[2].val, reg_rhs));
            } else {
                REQUIRE_OP_MODE(i, 2, AM_REF);
            }
            break;
        }

        case TYPE_I: {
            REQUIRE_OP_COUNT(i, 1);
            REQUIRE_OP_MODE(i, 0, AM_REG);
            emit_word(builder, encode_i(conf.code, i->ops[0].reg));
            break;
        }

        default: {
            asm_error(i->line_num, "unimplemented instruction %s", i->name);
            builder->failed = true;
            return false;
        }
    }

    return true;
}

bool codegen(FILE *out, Source *source) {
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
                builder.failed = true;
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

    if (builder.failed) return false;
    else return true;
}