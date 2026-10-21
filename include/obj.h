#ifndef OBJ_H
#define OBJ_H

#include <stdint.h>

typedef uint32_t section_id_t; // 0 == undefined
typedef uint32_t symbol_id_t;
typedef uint32_t stroff_t; // offset into string pool
typedef uint32_t dataoff_t; // offset into data pool
typedef enum {
    SYMFLAG_GLOBAL = 1 << 0,
} symbol_flag_t;
typedef enum {
    FLAG_R = 1 << 7,
    FLAG_W = 1 << 6,
    FLAG_X = 1 << 5,
    FLAG_U = 1 << 4,
} section_flag_t;

typedef enum {
    RELOC_ABS32 = 1,
    RELOC_PCREL26,
    RELOC_PCREL13,
    RELOC_HI16,
    RELOC_LO16,
} reloc_type_t;

typedef struct {
    uint32_t magic; // "OR\0N"
    uint32_t version; // maj8 min8 isa8 pad8
    uint32_t sections;
    uint32_t syms;
    uint32_t relocations;
    uint32_t data_pool_size;
    uint32_t string_pool_size;
} obj_header_t;

typedef struct {
    stroff_t name;
    dataoff_t data;
    uint32_t align;
    uint32_t size;
    uint8_t flags; // RWX, uninit
    section_id_t id;
} obj_section_t;

typedef struct {
    stroff_t name;
    uint32_t offset;
    section_id_t section;
    symbol_flag_t flags; // global
} obj_symbol_t;

typedef struct {
    uint32_t offset;
    section_id_t section;
    symbol_id_t symbol;
    int32_t addend;
    reloc_type_t type;
} obj_reloc_t;

typedef uint32_t addr_t;

typedef struct {
    uint32_t magic; // "OR\0N"
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

#endif