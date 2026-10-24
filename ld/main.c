#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
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

static inline void fclosep(FILE **f) {if (f) fclose(*f);}

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
    if (fseek(f, 0, SEEK_SET) != 0) return false;

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

    obj.sections.len = obj.header.sections;
    obj.symbols.len = obj.header.symbols;
    obj.relocs.len = obj.header.relocs;
    obj.data_pool.len = obj.header.data_pool_size;
    obj.string_pool.len = obj.header.string_pool_size;

    *out_obj = obj;

    return true;
}

bool validate_object(InputObject *obj) {
    assert(obj != NULL);

    for (uint32_t i = 0; i < obj->header.sections; i++) {
        obj_section_t section = obj->sections.data[i];
        if (section.id == 0) return false;
        if (section.id != i + 1) return false;
        if ((section.flags & FLAG_U) == 0 && (section.data > obj->data_pool.cap || section.size > obj->data_pool.cap - section.data)) return false;
        if (section.align == 0 || (section.align & (section.align - 1)) != 0) return false;
        if (section.name >= obj->string_pool.cap) return false;
    }

    for (uint32_t i = 0; i < obj->header.symbols; i++) {
        obj_symbol_t symbol = obj->symbols.data[i];
        if (symbol.name >= obj->string_pool.cap) return false;
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
} OutputSection;

typedef struct {
    uint32_t output_section; // index into LinkContext.sections
    uint32_t offset;         // offset within the output section
} SectionPlacement;

typedef struct {
    InputObject *objects;
    size_t object_count;

    da_type(OutputSection) sections;

    da_type(da_type(SectionPlacement)) placements;
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

static bool create_output_section(LinkContext *ctx, const InputObject *obj, const obj_section_t *input, uint32_t *out_index) {
    assert(ctx != NULL); assert(obj != NULL); assert(input != NULL); assert(out_index != NULL);

    char *input_name = (char*)&obj->string_pool.data[input->name];
    
    OutputSection out = {
        .align = input->align,
        .flags = input->flags,
        .memory_size = 0,
    };

    da_init(&out.data);

    out.name = malloc(strlen(input_name) + 1);
    if (!out.name) return false;
    strcpy(out.name, input_name);

    da_push(&ctx->sections, out);
    *out_index = ctx->sections.len - 1;

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
        if (!create_output_section(ctx, obj, input, &output_index)) return false;

    OutputSection *output = &ctx->sections.data[output_index];
    if (output->flags != input->flags) return false;

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

        output->data.len = end;
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

    da_resize(&ctx->placements, object_count);
    ctx->placements.len = object_count;

    for (size_t i = 0; i < object_count; i++) {
        da_init(&ctx->placements.data[i]);

        size_t count = objects[i].sections.len;
        da_resize(&ctx->placements.data[i], count);
        ctx->placements.data[i].len = count;

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

        if (section->align == 0 ||
            (section->align & (section->align - 1)) != 0)
            return false;

        uint64_t alignment = section->align;
        uint64_t start = (cursor + alignment - 1) & ~(alignment - 1);

        if (start > UINT32_MAX)
            return false;

        uint64_t end = start + section->memory_size;
        if (end > address_space_end)
            return false;

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