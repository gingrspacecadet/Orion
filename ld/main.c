#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "obj.h"

obj_section_t g_sections[UINT8_MAX];
size_t g_section_count;
obj_symbol_t g_symbols[UINT8_MAX];
size_t g_symbol_count;
obj_reloc_t g_relocs[UINT8_MAX];
size_t g_reloc_count;
uint8_t g_data_pool[UINT8_MAX];
size_t g_data_pool_size;
uint8_t g_string_pool[UINT8_MAX];
size_t g_string_pool_size;

int find_or_create_section(char *name) {
    for (size_t i = 0; i < g_section_count; i++) {
        if (strcmp((char*)&g_string_pool[g_sections[i].name], name) == 0) {
            return i;
        }
    }

    strcpy((char*)&g_string_pool[g_string_pool_size], name);
    g_sections[g_section_count].name = g_string_pool_size;
    g_sections[g_section_count].size = 0;
    g_sections[g_section_count].data = g_data_pool_size;
    g_string_pool_size += strlen(name) + 1;
    return g_section_count++;
}

bool link_file(char *name) {
    FILE *f = fopen(name, "rb");
    if (!f) {
        fprintf(stderr, "Linker Error: Failed to open file %s\n", name);
        return false;
    }

    obj_header_t header;
    if (fread(&header, sizeof(obj_header_t), 1, f) != 1 || header.magic != OBJ_MAGIC) {
        fprintf(stderr, "Linker Error: Invalid or corrupt object file %s\n", name);
        fclose(f);
        return false;
    }

    obj_section_t _sections[header.sections];
    obj_symbol_t _symbols[header.syms];
    obj_reloc_t _relocs[header.relocations];

    fread(_sections, sizeof(obj_section_t), header.sections, f);
    fread(_symbols, sizeof(obj_symbol_t), header.syms, f);
    fread(_relocs, sizeof(obj_reloc_t), header.relocations, f);

    uint8_t data_pool[header.data_pool_size];
    uint8_t string_pool[header.string_pool_size];

    fseek(f, sizeof(obj_header_t) + sizeof(obj_section_t) * header.sections + sizeof(obj_symbol_t) * header.syms + sizeof(obj_reloc_t) * header.relocations, SEEK_SET);
    fread(data_pool, sizeof(uint8_t), header.data_pool_size, f);
    fread(string_pool, sizeof(uint8_t), header.string_pool_size, f);
    fseek(f, sizeof(obj_header_t), SEEK_SET);

    section_id_t local_to_global_idx[UINT8_MAX];
    uint32_t local_base_offsets[UINT8_MAX];

    for (uint32_t i = 0; i < header.sections; i++) {
        obj_section_t section = _sections[i];

        int global_idx = find_or_create_section((char*)&string_pool[section.name]);
        local_to_global_idx[section.id] = global_idx;
        local_base_offsets[section.id] = g_sections[global_idx].size;

        if (section.size > 0) {
            memcpy(&g_data_pool[g_data_pool_size], &data_pool[section.data], section.size);
            g_data_pool_size += section.size;
            g_sections[global_idx].size += section.size;
        }
    }

    for (uint32_t i = 0; i < header.syms; i++) {
        obj_symbol_t sym = _symbols[i];

        int global_idx = local_to_global_idx[sym.section];

        for (size_t g = 0; g < g_symbol_count; g++) {
            if (strcmp((char*)&g_string_pool[g_symbols[g].name], (char*)&string_pool[sym.name]) == 0) {
                fprintf(stderr, "Linker Error: Redefinition of symbol '%s'\n", &string_pool[sym.name]);
                fclose(f);
                return false;
            }
        }

        strcpy((char*)&g_string_pool[g_string_pool_size], (char*)&string_pool[sym.name]);
        
        obj_symbol_t gsym = sym;
        gsym.section = local_to_global_idx[sym.section];
        gsym.offset += local_base_offsets[sym.section];
        gsym.name = g_string_pool_size;
        g_symbols[g_symbol_count++] = gsym;
        g_string_pool_size += strlen((char*)&string_pool[sym.name]) + 1;
    }

    for (uint32_t i = 0; i < header.relocations; i++) {
        obj_reloc_t reloc = _relocs[i];

        int global_idx = local_to_global_idx[reloc.section];

        g_relocs[g_reloc_count++] = (obj_reloc_t){
            .addend = reloc.addend,
            .offset = local_base_offsets[reloc.section] + reloc.offset,
            .section = global_idx,
            .type = reloc.type
        };
    }

    fclose(f);

    return true;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s file1.o file2.o ...\n", argv[0]);
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (!link_file(argv[i])) {
            return 1;
        }
    }

    for (size_t i = 0; i < g_section_count; i++) {
        printf("Section %zu name %s\n", i, &g_string_pool[g_sections[i].name]);
    }

    for (size_t i = 0; i < g_symbol_count; i++) {
        printf("Symbol %zu name %s\n", i, &g_string_pool[g_symbols[i].name]);
    }

    // patch();

    // emit();

    return 0;
}

/*

THE PLAN

two-pass linking

1st pass: open all files, collect sections, symbols, and relocations
2nd pass: apply relocations

emit executable

*/