#include <stdio.h>
#include <stddef.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include "obj.h"
#include "da.h"

typedef struct {
    obj_header_t header;
    da_type(obj_section_t) sections;
    da_type(obj_symbol_t) symbols;
    da_type(obj_reloc_t) relocs;
    da_type(uint8_t) data_pool;
    da_type(uint8_t) string_pool;
    char *name;
} InputObject;

#define CHECK_ADD(a, b, out) \
    (!__builtin_add_overflow((a), (b), out))

#define CHECK_MUL(a, b, out) \
    (!__builtin_mul_overflow((a), (b), out))

static inline void fclosep(FILE **f) {if (f && *f) fclose(*f);}

bool read_object(char *filename, InputObject *out_obj) {
    assert(out_obj != NULL);
    assert(filename != NULL);

    FILE *f __attribute__((cleanup(fclosep))) = fopen(filename, "rb");
    if (!f) return false;

    InputObject obj = {.name = filename};
    if (fread(&obj.header, sizeof(obj_header_t), 1, f) != 1) return false;

    if (obj.header.magic != OBJ_MAGIC) return false;
    if (obj.header.version != 0x01000100) return false;

    if (fseek(f, 0, SEEK_END) != 0) return false;
    long fsize = ftell(f);
    if (fsize < 0) return false;
    if (fseek(f, sizeof(obj_header_t), SEEK_SET) != 0) return false;

    size_t expected_size = 0;
    size_t tmp;

    expected_size = sizeof(obj_header_t);

    if (!CHECK_MUL(obj.header.sections, sizeof(obj_section_t), &tmp)) return false;
    if (!CHECK_ADD(expected_size, tmp, &expected_size)) return false;
    if (!CHECK_MUL(obj.header.symbols, sizeof(obj_symbol_t), &tmp)) return false;
    if (!CHECK_ADD(expected_size, tmp, &expected_size)) return false;
    if (!CHECK_MUL(obj.header.relocs, sizeof(obj_reloc_t), &tmp)) return false;
    if (!CHECK_ADD(expected_size, tmp, &expected_size)) return false;
    if (!CHECK_ADD(expected_size, obj.header.data_pool_size, &expected_size)) return false;
    if (!CHECK_ADD(expected_size, obj.header.string_pool_size, &expected_size)) return false;

    if (expected_size != (size_t)fsize)
        return false;

    da_resize(&obj.sections, obj.header.sections);
    da_resize(&obj.symbols, obj.header.symbols);
    da_resize(&obj.relocs, obj.header.relocs);
    da_resize(&obj.data_pool, obj.header.data_pool_size);
    da_resize(&obj.string_pool, obj.header.string_pool_size);

    if (fread(obj.sections.data, sizeof(obj_section_t), obj.header.sections, f) != obj.header.sections) return false;
    if (fread(obj.symbols.data, sizeof(obj_symbol_t), obj.header.symbols, f) != obj.header.symbols) return false;
    if (fread(obj.relocs.data, sizeof(obj_reloc_t), obj.header.relocs, f) != obj.header.relocs) return false;
    if (fread(obj.data_pool.data, sizeof(uint8_t), obj.header.data_pool_size, f) != obj.header.data_pool_size) return false;
    if (fread(obj.string_pool.data, sizeof(uint8_t), obj.header.string_pool_size, f) != obj.header.string_pool_size) return false;

    *out_obj = obj;

    return true;
}

#define INVALID(...) do { \
    fprintf(stderr, "invalid object \"%s\": ", obj->name); \
    fprintf(stderr, __VA_ARGS__); \
    fputc('\n', stderr); \
    return false; \
} while (0)

bool validate_object(InputObject *obj) {
    assert(obj != NULL);

    for (uint32_t i = 0; i < obj->header.sections; i++) {
        obj_section_t section = obj->sections.data[i];

        if (section.id == 0)
            INVALID("section %u has ID 0", i + 1);

        if (section.id != i + 1)
            INVALID("section %u has ID %u", i + 1, section.id);

        if ((section.flags & FLAG_U) == 0 && (section.data > obj->data_pool.len || section.size > obj->data_pool.len - section.data))
            INVALID("section %u data range is invalid: offset=%u size=%u pool_size=%zu", i + 1, section.data, section.size, obj->data_pool.len);

        if (section.align == 0 || (section.align & (section.align - 1)) != 0)
            INVALID("section %u has invalid alignment %u", i + 1, section.align);

        if (section.name >= obj->string_pool.len)
            INVALID("section %u has invalid name offset %u", i + 1, section.name);
    }

    for (uint32_t i = 0; i < obj->header.symbols; i++) {
        obj_symbol_t symbol = obj->symbols.data[i];
        if (symbol.name >= obj->string_pool.len) return false;
        if (symbol.section > obj->header.sections) return false;
        
        if (symbol.section != 0) {
            obj_section_t section = obj->sections.data[symbol.section - 1];
            if (symbol.offset > section.size) return false;
        }
    }

    for (uint32_t i = 0; i < obj->header.relocs; i++) {
        obj_reloc_t reloc = obj->relocs.data[i];
        if (reloc.type == RELOC_ERR) return false;
        if (reloc.section == 0 || reloc.section > obj->header.sections) return false;
        if (reloc.symbol >= obj->header.symbols) return false;
    }

    return true;
}

void destroy_object(InputObject *obj) {
    da_free(&obj->sections);
    da_free(&obj->symbols);
    da_free(&obj->relocs);
    da_free(&obj->data_pool);
    da_free(&obj->string_pool);
}

typedef struct {
    char *name;
    da_type(uint8_t) data;
    uint32_t memory_size;
    uint32_t align;
    uint8_t flags;
    addr_t address;
    bool has_address;
} OutputSection;

typedef struct {
    uint32_t output_section; // index into LinkContext.sections
    uint32_t offset;         // offset within the output section
} SectionPlacement;

typedef struct {
    addr_t address;
    bool resolved;
} ResolvedSymbol;

typedef struct {
    const char *name;
    addr_t address;
    size_t object_index;
    size_t symbol_index;
} GlobalSymbol;

typedef struct {
    InputObject *objects;
    size_t object_count;

    da_type(OutputSection) sections;

    da_type(da_type(SectionPlacement)) placements;

    da_type(da_type(ResolvedSymbol)) resolved_symbols;
    da_type(GlobalSymbol) global_symbols;
} LinkContext;

static bool find_output_section(const LinkContext *ctx, const char *name, uint32_t *out_index) {
    assert(ctx != NULL); assert(name != NULL); assert(out_index != NULL);

    for (size_t i = 0; i < ctx->sections.len; i++) {
        if (strcmp(ctx->sections.data[i].name, name) == 0) {
            *out_index = i;
            return true;
        }
    }

    return false;
}

static bool create_output_section(LinkContext *ctx, const char *name, const obj_section_t *input, uint32_t *out_index) {
    assert(ctx != NULL); assert(name != NULL);
    
    OutputSection out = {
        .align = input ? input->align : 0,
        .flags = input ? input->flags : 0,
        .memory_size = 0,
    };

    da_init(&out.data);

    out.name = malloc(strlen(name) + 1);
    if (!out.name) return false;
    strcpy(out.name, name);

    da_push(&ctx->sections, out);
    if (out_index) *out_index = ctx->sections.len - 1;

    return true;
}

static bool parse_script(LinkContext *ctx, const char *path) {
    FILE *f __attribute__((cleanup(fclosep))) = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Linker Error: failed to open linker script \"%s\"\n", path);
        return false;
    }

    char line[256];
    unsigned line_number = 0;

    while (fgets(line, sizeof(line), f)) {
        line_number++;

        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';

        char *name = strtok(line, " \t\r\n");
        if (!name) continue;

        char *address_text = strtok(NULL, " \t\r\n");
        if (strtok(NULL, " \t\r\n")) {
            fprintf(stderr, "%s:%u: unexpected extra token\n", path, line_number);
            return false;
        }

        uint32_t address = 0;
        if (address_text) {
            char *end;
            errno = 0;
            unsigned long value = strtoul(address_text, &end, 0);
            if (*address_text == '-' || *end != '\0' || errno == ERANGE || value > UINT32_MAX) {
                fprintf(stderr, "%s:%u: invalid section address \"%s\"\n", path, line_number, address_text);
                return false;
            }
            address = (uint32_t)value;
        }

        uint32_t index;
        if (find_output_section(ctx, name, &index)) {
            fprintf(stderr, "%s:%u: duplicate section \"%s\"\n", path, line_number, name);
            return false;
        }

        if (!create_output_section(ctx, name, NULL, &index)) {
            fprintf(stderr, "%s:%u: failed to create section \"%s\"\n", path, line_number, name);
            return false;
        }

        OutputSection *section = &ctx->sections.data[index];
        section->address = address;
        section->has_address = address_text != NULL;
    }

    return true;
}

static bool align_up_u32(uint32_t value, uint32_t alignment, uint32_t *out) {
    uint64_t tmp;
    if (!CHECK_ADD(value, alignment, &tmp)) return false;
    tmp--;
    tmp /= alignment;
    if (!CHECK_MUL(tmp, alignment, &tmp)) return false;
    
    if (tmp > UINT32_MAX) return false;
    *out = tmp;
    return true;
}

static bool append_input_section(LinkContext *ctx, size_t object_index, size_t section_index) {
    assert(ctx != NULL);

    InputObject *obj = &ctx->objects[object_index];
    obj_section_t *input = &obj->sections.data[section_index];

    const char *name = (const char *)&obj->string_pool.data[input->name];

    uint32_t output_index;
    if (!find_output_section(ctx, name, &output_index))
        if (!create_output_section(ctx, name, input, &output_index)) return false;

    OutputSection *output = &ctx->sections.data[output_index];
    if (output->align == 0) {
        output->align = input->align;
        output->flags = input->flags;
    } else if (output->flags != input->flags) {
        fprintf(stderr, "incompatible flags merging section \"%s\"\n", name);
        return false;
    }

    uint32_t offset;
    if (!align_up_u32(output->memory_size, input->align, &offset)) return false;

    uint32_t end;
    if (!CHECK_ADD(offset, input->size, &end)) return false;

    if ((input->flags & FLAG_U) == 0) {
        da_resize(&output->data, end);
    
        if (offset > output->data.len)
            memset(output->data.data + output->data.len, 0, offset - output->data.len);
        
        if (input->size != 0)
            memcpy(output->data.data + offset, obj->data_pool.data + input->data, input->size);
    }
    
    output->memory_size = end;
    if (input->align > output->align)
        output->align = input->align;

    SectionPlacement placement = {
        .output_section = output_index,
        .offset = offset,
    };

    ctx->placements.data[object_index].data[section_index] = placement;

    return true;
}

bool link_context_init(LinkContext *ctx, InputObject *objects, size_t object_count) {
    assert(ctx != NULL); assert(objects != NULL);

    ctx->objects = objects;
    ctx->object_count = object_count;

    da_init(&ctx->sections);
    da_init(&ctx->global_symbols);
    da_init(&ctx->placements);

    da_resize(&ctx->placements, object_count);

    for (size_t i = 0; i < object_count; i++) {
        memset(&ctx->placements.data[i], 0, sizeof(ctx->placements.data[i]));

        da_init(&ctx->placements.data[i]);

        size_t count = objects[i].sections.len;
        da_resize(&ctx->placements.data[i], count);

        for (size_t j = 0; j < count; j++) {
            ctx->placements.data[i].data[j] = (SectionPlacement) {
                .output_section = UINT32_MAX,
                .offset = 0,
            };
        }
    }

    return true;
}

void link_context_destroy(LinkContext *ctx) {
    assert(ctx != NULL);

    for (size_t i = 0; i < ctx->sections.len; i++) {
        free(ctx->sections.data[i].name);
        da_free(&ctx->sections.data[i].data);
    }
    da_free(&ctx->sections);

    for (size_t i = 0; i < ctx->placements.len; i++) {
        da_free(&ctx->placements.data[i]);
    }

    da_free(&ctx->placements);

    return;
}

bool merge_sections(LinkContext *ctx) {
    assert(ctx != NULL);

    for (size_t i = 0; i < ctx->object_count; i++) {
        InputObject *obj = &ctx->objects[i];

        for (size_t j = 0; j < obj->sections.len; j++) {
            if (!append_input_section(ctx, i, j))
                return false;
        }
    }

    return true;
}

bool layout_sections(LinkContext *ctx, addr_t base_address) {
    assert(ctx != NULL);

    uint64_t cursor = base_address;
    const uint64_t address_space_end = (uint64_t)UINT32_MAX + 1;

    for (size_t i = 0; i < ctx->sections.len; i++) {
        OutputSection *section = &ctx->sections.data[i];

        uint32_t alignment = section->align ? section->align : 1;
        if ((alignment & (alignment - 1)) != 0) {
            fprintf(stderr, "Invalid alignment for section \"%s\": %u\n", section->name, alignment);
            return false;
        }

        uint64_t start;
        if (section->has_address) {
            start = section->address;
            if (start % alignment != 0) {
                fprintf(stderr, "section \"%s\": address 0x%08llX violates alignment %llu\n", section->name, (unsigned long long)start, (unsigned long long)alignment);
                return false;
            }
        } else {
            start = (cursor + alignment - 1) & ~(uint64_t)(alignment - 1);
        }

        if (start > UINT32_MAX) {
            fprintf(stderr, "section \"%s\": start address exceeds 32-bit address space\n", section->name);
            return false;
        }

        uint64_t end = start + section->memory_size;
        if (end > address_space_end) {
            fprintf(stderr, "section \"%s\": address range exceeds 32-bit address space\n", section->name);
            return false;
        }

        if (section->memory_size != 0) {
            for (size_t j = 0; j < i; j++) {
                const OutputSection *previous = &ctx->sections.data[j];
                if (previous->memory_size == 0)
                    continue;

                uint64_t previous_end = (uint64_t)previous->address + previous->memory_size;
                if (start < previous_end && previous->address < end) {
                    fprintf(stderr, "sections \"%s\" and \"%s\" overlap\n", previous->name, section->name);
                    return false;
                }
            }
        }


        section->address = (addr_t)start;
        cursor = end;
    }

    return true;
}

bool get_section_placement(const LinkContext *ctx, size_t object_index, section_id_t input_section_id, SectionPlacement *out) {
    assert(ctx != NULL); assert(out != NULL);

    if (object_index >= ctx->object_count) return false;

    if (input_section_id == 0) return false;

    const InputObject *obj = &ctx->objects[object_index];
    if (input_section_id > obj->sections.len) return false;

    SectionPlacement placement = ctx->placements.data[object_index].data[input_section_id - 1];

    if (placement.output_section == UINT32_MAX) return false;

    if (placement.output_section >= ctx->sections.len) return false;

    *out = placement;
    return true;
}

bool resolve_defined_symbols(LinkContext *ctx) {
    da_init(&ctx->resolved_symbols);
    da_resize(&ctx->resolved_symbols, ctx->object_count);

    for (size_t i = 0; i < ctx->object_count; i++) {
        InputObject object = ctx->objects[i];

        memset(&ctx->resolved_symbols.data[i], 0, sizeof(da_type(int)));
        da_init(&ctx->resolved_symbols.data[i]);
        da_resize(&ctx->resolved_symbols.data[i], object.symbols.len);

        for (size_t j = 0; j < object.symbols.len; j++) {
            obj_symbol_t symbol = object.symbols.data[j];
            SectionPlacement placement;

            if (symbol.section == 0)
                continue;
            
            if (!get_section_placement(ctx, i, symbol.section, &placement)) return false;

            OutputSection section = ctx->sections.data[placement.output_section];

            uint64_t address;
            if (!CHECK_ADD(section.address, placement.offset, &address)) return false;
            if (!CHECK_ADD(address, symbol.offset, &address)) return false;

            if (address > UINT32_MAX)
                return false;

            ctx->resolved_symbols.data[i].data[j] = (ResolvedSymbol){
                .address = address,
                .resolved = true,
            };
        }
    }

    return true;
}

bool find_global_symbol(const LinkContext *ctx, const char *name, size_t *out_index) {
    assert(ctx != NULL); assert(name != NULL);

    for (size_t i = 0; i < ctx->global_symbols.len; i++) {
        if (strcmp(ctx->global_symbols.data[i].name, name) == 0) {
            if (out_index) *out_index = i;
            return true;
        }
    }

    return false;
}

bool build_global_symbol_table(LinkContext *ctx) {
    for (size_t i = 0; i < ctx->object_count; i++) {
        InputObject object = ctx->objects[i];

        for (size_t j = 0; j < object.symbols.len; j++) {
            obj_symbol_t symbol = object.symbols.data[j];

            if ((symbol.flags & SYMFLAG_GLOBAL) == 0)
                continue;

            if (symbol.section == 0)
                continue;

            if (find_global_symbol(ctx, (char *)&object.string_pool.data[symbol.name], NULL)) {
                fprintf(stderr, "Duplicate symbol \"%s\"\n", (char *)&object.string_pool.data[symbol.name]);
                return false;
            }

            ResolvedSymbol resolved = ctx->resolved_symbols.data[i].data[j];

            if (!resolved.resolved)
                return false;

            GlobalSymbol out = {
                .address = resolved.address,
                .name = (char *)&object.string_pool.data[symbol.name],
                .object_index = i,
                .symbol_index = j,
            };

            da_push(&ctx->global_symbols, out);
        }
    }

    return true;
}

bool resolve_undefined_symbols(LinkContext *ctx) {
    assert(ctx != NULL);

    for (size_t i = 0; i < ctx->object_count; i++) {
        InputObject object = ctx->objects[i];
        for (size_t j = 0; j < object.symbols.len; j++) {
            obj_symbol_t symbol = object.symbols.data[j];

            if (symbol.section != 0)
                continue;

            size_t index;
            if (!find_global_symbol(ctx, (char *)&object.string_pool.data[symbol.name], &index)) {
                fprintf(stderr, "Failed to find global symbol \"%s\"\n", (char *)&object.string_pool.data[symbol.name]);
                return false;
            }

            ResolvedSymbol *resolved = &ctx->resolved_symbols.data[i].data[j];
            resolved->address = ctx->global_symbols.data[index].address;
            resolved->resolved = true;
        }
    }

    return true;
}

static bool add_linker_symbol(LinkContext *ctx, const char *name, addr_t address) {
    if (find_global_symbol(ctx, name, NULL)) {
        fprintf(stderr, "internal linker symbol collision: %s\n", name);
        return false;
    }

    GlobalSymbol sym = {
        .name = strdup(name),
        .address = address,
        .object_index = SIZE_MAX,
        .symbol_index = SIZE_MAX,
    };

    da_push(&ctx->global_symbols, sym);
    return true;
}

static uint32_t calculate_image_base(LinkContext *ctx) {
    uint32_t lowest = UINT32_MAX;
    for (size_t i = 0; i < ctx->sections.len; i++) {
        OutputSection section = ctx->sections.data[i];
        if (section.memory_size == 0) continue;
        if (section.address < lowest)
            lowest = section.address;
    }

    return lowest;
}

static uint32_t calculate_image_size(LinkContext *ctx) {
    uint32_t total = 0;
    for (size_t i = 0; i < ctx->sections.len; i++) {
        OutputSection section = ctx->sections.data[i];
        total += section.memory_size;
    }

    return total;
}

static bool create_section_boundary_symbols(LinkContext *ctx) {
    char start_name[256];
    char end_name[256];

    for (size_t i = 0; i < ctx->sections.len; i++) {
        OutputSection *section = &ctx->sections.data[i];

        const char *base = section->name;

        while (*base == '.')
            base++;

        snprintf(start_name, sizeof(start_name), "__%s_start", base);
        snprintf(end_name, sizeof(end_name), "__%s_end", base);

        uint64_t end;
        if (!CHECK_ADD(section->address, section->memory_size, &end))
            return false;

        if (!add_linker_symbol(ctx, start_name, section->address))
            return false;

        if (!add_linker_symbol(ctx, end_name, (addr_t)end))
            return false;
    }

    if (!add_linker_symbol(ctx, "__exe_start", calculate_image_base(ctx)))
        return false;

    if (!add_linker_symbol(ctx, "__exe_end", calculate_image_base(ctx) + calculate_image_size(ctx)))
        return false;

    return true;
}

bool resolve_symbols(LinkContext *ctx) {
    assert(ctx != NULL);

    if (!resolve_defined_symbols(ctx)) {
        fprintf(stderr, "Failed to resolve defined symbols\n");
        return false;
    }

    if (!build_global_symbol_table(ctx)) {
        fprintf(stderr, "Failed to build global symbol table\n");
        return false;
    }

    if (!create_section_boundary_symbols(ctx)) {
        fprintf(stderr, "Failed to create section boundary symbols\n");
        return false;
    }

    if (!resolve_undefined_symbols(ctx)) {
        fprintf(stderr, "Failed to resolve undefined symbols\n");
        return false;
    }

    return true;
}

bool read_u32_le(const uint8_t *data, size_t len, size_t offset, uint32_t *out) {
    assert(data != NULL); assert(out != NULL);

    if (offset > len || len - offset < 4)
        return false;

    *out = (uint32_t)data[offset]
         | ((uint32_t)data[offset + 1] << 8)
         | ((uint32_t)data[offset + 2] << 16)
         | ((uint32_t)data[offset + 3] << 24);

    return true;
}

bool write_u32_le(uint8_t *data, size_t len, size_t offset, uint32_t value) {
    assert(data != NULL);

    if (offset > len || len - offset < 4)
        return false;

    data[offset]     = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
    data[offset + 2] = (uint8_t)(value >> 16);
    data[offset + 3] = (uint8_t)(value >> 24);

    return true;
}

bool apply_relocation(LinkContext *ctx, size_t object_index, const obj_reloc_t *reloc) {
    __auto_type resolved_symbols = ctx->resolved_symbols.data[object_index];
    if (reloc->symbol >= resolved_symbols.len) {
        fprintf(stderr, "failed to apply relocation: symbol id %u out of range\n", reloc->symbol);
        return false;
    }

    const ResolvedSymbol *symbol = &resolved_symbols.data[reloc->symbol];

    if (!symbol->resolved) {
        fprintf(stderr, "failed to apply relocation: unresolved symbol\n");
        return false;
    }

    SectionPlacement placement;
    if (!get_section_placement(ctx, object_index, reloc->section, &placement)) {
        fprintf(stderr, "failed to apply relocation: unknown placement\n");
        return false;
    }

    OutputSection *output = &ctx->sections.data[placement.output_section];

    if (output->flags & FLAG_U) {
        fprintf(stderr, "failed to apply relocation: cannot write in uninitialised section\n");
        return false;
    }

    uint32_t Q;
    if (!CHECK_ADD(placement.offset, reloc->offset, &Q)) {
        fprintf(stderr, "failed to apply relocation: offset out of 32-bit bounds\n");
        return false;
    }

    uint32_t word;
    if (!read_u32_le(output->data.data, output->data.len, Q, &word)) {
        fprintf(stderr, "failed to apply relocation: failed to read word\n");
        fprintf(stderr, "len %zu offset %u\n", output->data.len, Q);
        return false;
    }

    int64_t target = (int64_t)symbol->address + reloc->addend;

    if (target < 0 || target > UINT32_MAX) {
        fprintf(stderr, "failed to apply relocation: target out-of-bounds\n");
        return false;
    }

    switch (reloc->type) {
        case RELOC_ABS32:
            word = (uint32_t)target;
            break;

        case RELOC_HI16:
            word = (word & ~UINT32_C(0x0003FFFC))
                 | (((uint32_t)target >> 16) << 2);
            break;

        case RELOC_LO16:
            word = (word & ~UINT32_C(0x0003FFFC))
                 | ((((uint32_t)target) & 0xFFFFu) << 2);
            break;

        // bit more complicated lol
        case RELOC_PCREL26: {
            uint64_t pc = (uint64_t)output->address + Q;
            int64_t delta = target - (int64_t)(pc + 4);

            if (delta % 4 != 0)
                return false;

            int64_t displacement = delta / 4;

            if (displacement < -(INT64_C(1) << 25) || displacement > (INT64_C(1) << 25) - 1)
                return false;

            word = (word & ~UINT32_C(0x03FFFFFF))
                 | ((uint32_t)displacement & UINT32_C(0x03FFFFFF));
            break;
        }

        case RELOC_PCREL13: {
            uint64_t pc = (uint64_t)output->address + Q;
            int64_t delta = target - (int64_t)(pc + 4);

            if (delta % 4 != 0)
                return false;

            int64_t displacement = delta / 4;

            if (displacement < -(INT64_C(1) << 12) || displacement > (INT64_C(1) << 12) - 1)
                return false;

            word = (word & ~UINT32_C(0x00003FFE))
                 | (((uint32_t)displacement << 1) & UINT32_C(0x00003FFE));
            break;
        }

        default:
            return false; // Todo!
    }

    return write_u32_le(output->data.data, output->data.len, Q, word);
}

bool apply_relocations(LinkContext *ctx) {
    for (size_t i = 0; i < ctx->object_count; i++) {
        InputObject *obj = &ctx->objects[i];

        for (size_t j = 0; j < obj->relocs.len; j++) {
            if (!apply_relocation(ctx, i, &obj->relocs.data[j]))
                return false;
        }
    }

    return true;
}

bool write_executable(const LinkContext *ctx, const char *path, addr_t entry) {
    assert(ctx != NULL); assert(path != NULL);

    FILE *f __attribute__((cleanup(fclosep))) = fopen(path, "wb");
    if (!f) return false;

    exe_header_t header = {
        .magic = EXE_MAGIC,
        .version = 0x01000100,
        .entry = entry,
        .sections = ctx->sections.len,
    };

    if (fwrite(&header, sizeof(exe_header_t), 1, f) != 1)
        return false;
    
    uint64_t total_offset = sizeof(exe_header_t) + (uint64_t)ctx->sections.len * sizeof(exe_section_t);
    for (size_t i = 0; i < ctx->sections.len; i++) {
        OutputSection section = ctx->sections.data[i];

        exe_section_t exesection = {
            .address = section.address,
            .align = section.align,
            .file_size = section.data.len,
            .flags = section.flags & (FLAG_R | FLAG_W | FLAG_X),
            .memory_size = section.memory_size,
            .offset = total_offset,
        };

        total_offset += section.data.len;

        if (fwrite(&exesection, sizeof(exe_section_t), 1, f) != 1)
            return false;
    }

    for (size_t i = 0; i < ctx->sections.len; i++) {
        size_t len = ctx->sections.data[i].data.len;

        if (len != 0 && fwrite(ctx->sections.data[i].data.data, 1, len, f) != len)
            return false;
    }

    return true;
}

typedef struct {
    const char *script_path;
    const char *output_path;
    da_type(char *) input_paths;
    bool flat;
} LinkOptions;

void print_usage(FILE *stream, const char *program) {
    assert(stream != NULL); assert(program != NULL);

    fprintf(stream, "Usage: %s [-f] [-T script.ld] [-o output.oxe] input.o [input.o ...]\n", program);
}

int parse_args(int argc, char **argv, LinkOptions *out) {
    assert(argv != NULL); assert(out != NULL);

    da_init(&out->input_paths);
    out->output_path = "a.oxe";

    int opt;

    while ((opt = getopt(argc, argv, ":T:o:fh")) != -1) {
        switch (opt) {
            case 'T':
                out->script_path = optarg;
                break;

            case 'o':
                out->output_path = optarg;
                break;

            case 'f':
                out->flat = true;
                break;

            case ':':
                fprintf(stderr, "%s: option -%c requires an argument\n", argv[0], optopt);
                print_usage(stderr, argv[0]);
                return -1;
            
            case 'h':
                fprintf(stderr, "%s: help: ", argv[0]);
                print_usage(stderr, argv[0]);
                return 0;
            
            case '?':
                fprintf(stderr, "%s: unknown option '-%c'\n", argv[0], optopt);
                print_usage(stderr, argv[0]);
                return -1;
        }
    }

    if (optind == argc) {
        print_usage(stderr, argv[0]);
        return -1;
    }

    for (; optind < argc; optind++) {
        da_push(&out->input_paths, argv[optind]);
    }

    return 1;
}

int main(int argc, char **argv) {
    LinkOptions options = {0};

    switch (parse_args(argc, argv, &options)) {
        case -1: return 1;
        case 0: return 0;

        default: break;
    }

    da_type(InputObject) objects = {0};
    da_init(&objects);
    da_resize(&objects, options.input_paths.len);

    for (size_t i = 0; i < options.input_paths.len; i++) {
        if (!read_object(options.input_paths.data[i], &objects.data[i])) {
            fprintf(stderr, "%s: failed to read object \"%s\"\n", argv[0], options.input_paths.data[i]);
            return 1;
        }
        if (!validate_object(&objects.data[i])) {
            fprintf(stderr, "%s: failed to validate object \"%s\"\n", argv[0], options.input_paths.data[i]);
            return 1;
        }
    }

    LinkContext ctx = {0};

    
    if (!link_context_init(&ctx, objects.data, objects.len)) {
        fprintf(stderr, "%s: failed to initialise linker context\n", argv[0]);
        return 1;
    }

    if (options.script_path && !parse_script(&ctx, options.script_path)) {
        return 1;
    }

    if (!merge_sections(&ctx)) {
        fprintf(stderr, "%s: failed to merge sections\n", argv[0]);
        return 1;
    }

    if (!layout_sections(&ctx, 0x1000)) {
        fprintf(stderr, "%s: failed to lay out sections\n", argv[0]);
        return 1;
    }

    if (!resolve_symbols(&ctx)) {
        fprintf(stderr, "%s: failed to resolve symbols\n", argv[0]);
        return 1;
    }
    
    if (!apply_relocations(&ctx)) {
        fprintf(stderr, "%s: failed to apply relocations\n", argv[0]);
        return 1;
    }

    size_t entry_index;
    if (!find_global_symbol(&ctx, "_start", &entry_index)) {
        fprintf(stderr, "%s: missing entry symbol \"_start\"\n", argv[0]);
        return 1;
    }

    GlobalSymbol entry = ctx.global_symbols.data[entry_index];

    if (options.flat) {
        FILE *out __attribute__((cleanup(fclosep))) = fopen(options.output_path, "wb");
        if (!out) return 1;

        for (size_t i = 0; i < ctx.sections.len; i++) {
            uint32_t image_base = calculate_image_base(&ctx);

            OutputSection *section = &ctx.sections.data[i];
            long offset = (long)(section->address - image_base);
            if (fseek(out, offset, SEEK_SET) != 0) {
                fprintf(stderr, "%s: failed to seek while writing flat output\n", argv[0]);
                return 1;
            }

            size_t len = section->data.len;
            if (len != 0 && fwrite(section->data.data, 1, len, out) != len)
                return 1;
        }
    } else {
        if (!write_executable(&ctx, options.output_path, entry.address)) {
            fprintf(stderr, "%s: failed to write executable\n", argv[0]);
            return 1;
        }
    }

    return 0;
}