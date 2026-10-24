#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "obj.h"

#if !defined(__GNUC__) && !defined(__clang__)
#   error This project requires GNU C extensions.
#endif

#if __STDC_VERSION__ < 201112L
#   error C11 required.
#endif

#define da_type(T)      \
    struct {            \
        T *data;        \
        size_t len;     \
        size_t cap;     \
    }

#define da_resize(da, elems)                                                        \
    do {                                                                            \
        __auto_type _da = da;                                                       \
        size_t _n = elems;                                                          \
        if (_n <= _da->cap) break;                                                  \
        assert(_n <= SIZE_MAX / sizeof(*_da->data));                                \
        typeof(_da->data) new_ptr = realloc(_da->data, _n * sizeof(*_da->data));    \
        assert(new_ptr);                                                            \
        _da->data = new_ptr;                                                        \
        _da->cap = _n;                                                              \
    } while (0)

#define da_free(da)                 \
    do {                            \
        __auto_type _da = da;       \
        free(_da->data);            \
        _da->data = NULL;           \
        _da->len  = 0;              \
        _da->cap  = 0;              \
    } while (0)

#define da_push(da, value)                              \
    do {                                                \
        __auto_type _da = da;                           \
        __auto_type _value = (value);                   \
                                                        \
        if (_da->len == _da->cap)                       \
            da_resize(_da, _da.cap ? _da.cap * 2 : 16); \
                                                        \
        _da->data[_da->len++] = _value;                 \
    } while (0)

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
        if (section.data > obj->data_pool.cap || section.size > obj->data_pool.cap - section.data) return false;
        if (section.align == 0 || (section.align & (section.align - 1)) != 0) return false;
        if (section.name > obj->string_pool.cap) return false;
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