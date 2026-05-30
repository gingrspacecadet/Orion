#ifndef LINKER_H
#define LINKER_H

#include <stdint.h>
#include "obj.h"

#define MAX_SECTIONS             32
#define MAX_EXEC_SECTION_SIZE    1048576
#define MAX_GLOBAL_SYMBOLS       4096
#define MAX_TOTAL_RELOCS         4096

typedef struct {
    char name[32];
    uint32_t vma;
} SectionLayout;

typedef struct {
    char name[32];
    uint8_t buffer[MAX_EXEC_SECTION_SIZE];
    uint32_t size;
} ExecSection;

typedef struct {
    char name[32];
    uint32_t absolute_addr;
} GlobalSymbol;

typedef struct {
    ObjReloc reloc;
    int global_section_idx;
    uint32_t section_base_offset;
} DeferredReloc;

void link_object_file(const char *filename);
void resolve_relocations(void);
void write_executable(const char *out_path);
void parse_linker_script(const char *path);

#endif