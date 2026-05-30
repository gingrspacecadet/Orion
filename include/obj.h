#ifndef OBJ_H
#define OBJ_H

#include <stdint.h>
#include <stdbool.h>

/**
 * 
 * Object format layout:
 * - header
 * - .text
 * - .data
 * - symbol table
 * - relocation table
 * 
 */

#define OBJ_MAGIC   0x4F52004E  // "OR\0N"

typedef struct {
    uint32_t magic;
    uint32_t section_count;
    uint32_t sym_count;
    uint32_t reloc_count;
} ObjHeader;

typedef struct {
    char name[32];
    uint32_t size;
    int id;
} ObjSectionHeader;

typedef struct {
    char name[32];
    uint32_t offset;
    uint8_t section;
    uint8_t is_global;
} ObjSymbol;

typedef struct {
    uint32_t patch_offset;
    int patch_section;
    uint8_t patch_type;
    char symbol_name[32];
} ObjReloc;

#define RELOC_LO16      1
#define RELOC_HI16      2
#define RELOC_PC_REL    3

#endif