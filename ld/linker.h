#ifndef LINKER_H
#define LINKER_H

#include <stdint.h>
#include "obj.h"

#define MAX_EXEC_SIZE       65536
#define MAX_GLOBAL_SYMBOLS  4096
#define MAX_TOTAL_RELOCS    2048

extern uint32_t text_vma;
extern uint32_t data_vma;

extern uint8_t exec_text[MAX_EXEC_SIZE];
extern uint32_t exec_text_size;

extern uint8_t exec_data[MAX_EXEC_SIZE];
extern uint32_t exec_data_size;

typedef struct {
    char name[32];
    uint32_t absolute_addr;
} GlobalSymbol;

extern GlobalSymbol global_sym_table[MAX_GLOBAL_SYMBOLS];
extern int global_sym_count;

typedef struct {
    ObjReloc reloc;
    uint32_t text_base_offset;
} DeferredReloc;

extern DeferredReloc master_reloc_table[MAX_TOTAL_RELOCS];
extern int master_reloc_count;

void link_object_file(const char *filename);
void resolve_relocations(void);
void write_executable(const char *out_path);

#endif