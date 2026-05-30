#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "codegen.h"
#include "parser.h"
#include "lexer.h"
#include "obj.h"

// External declarations from your codegen.c
extern uint8_t text_section[];
extern uint32_t text_ptr;
extern uint8_t data_section[];
extern uint32_t data_ptr;
extern ObjSymbol symbol_table[];
extern int symbol_count;
extern ObjReloc reloc_table[];
extern int reloc_count;

void codegen(instr_array *instrs);

// Helper function to read an entire file into memory safely
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("Error opening source file");
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) {
        fprintf(stderr, "Fatal: Out of memory reading file.\n");
        exit(1);
    }

    size_t read_bytes = fread(buf, 1, size, f);
    buf[read_bytes] = '\0';

    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.s> [output.o]\n", argv[0]);
        return 1;
    }
    
    const char *input_path = argv[1];
    char *output_path;
    
    if (argc == 2) {
        const char *dot = strrchr(input_path, '.');
        if (!dot) {
            output_path = xmalloc(strlen(input_path) + 3);
            strcpy(output_path, input_path);
            strcat(output_path, ".o");
        } else {
            size_t base_len = (size_t)(dot - input_path);
            output_path = xmalloc(base_len + 3);
            strncpy(output_path, input_path, base_len);
            output_path[base_len] = '.';
            output_path[base_len + 1] = 'o';
            output_path[base_len + 2] = '\0';
        }
    } else {
        output_path = argv[2];
    }

    char *source = read_file(input_path);

    token_array tokens = lex(source);
    free(source);

    instr_array instrs = parse(&tokens);
    token_array_free(&tokens); 

    // Implicitly default to .text so code without directives has a home
    switch_section(".text");

    codegen(&instrs);
    instr_array_free(&instrs);
    
    FILE *out = fopen(output_path, "wb");
    if (!out) {
        perror("Error creating output object file");
        return 1;
    }

    // 1. Serialize Main File Header
    ObjHeader header = {
        .magic = OBJ_MAGIC,
        .section_count = (uint32_t)section_count,
        .sym_count = (uint32_t)symbol_count,
        .reloc_count = (uint32_t)reloc_count
    };
    fwrite(&header, sizeof(ObjHeader), 1, out);

    // 2. Serialize Section Headers (Metadata table for the linker)
    for (int i = 0; i < section_count; i++) {
        ObjSectionHeader sh = {
            .size = sections[i].ptr,
            .id = sections[i].id
        };
        strncpy(sh.name, sections[i].name, 31);
        fwrite(&sh, sizeof(ObjSectionHeader), 1, out);
    }

    // 3. Serialize Section Binary Payloads
    for (int i = 0; i < section_count; i++) {
        if (sections[i].ptr > 0) {
            fwrite(sections[i].buffer, 1, sections[i].ptr, out);
        }
    }

    // 4. Serialize Symbol Table
    if (symbol_count > 0) {
        fwrite(symbol_table, sizeof(ObjSymbol), symbol_count, out);
    }
    
    // 5. Serialize Relocation Table (Now tracking patch_section IDs!)
    if (reloc_count > 0) {
        fwrite(reloc_table, sizeof(ObjReloc), reloc_count, out);
    }

    fclose(out);
    
    // Summary output is now perfectly dynamic!
    printf("Wrote output to %s\n", output_path);
    for (int i = 0; i < section_count; i++) {
        printf(" -> Section '%s' (ID %d) size: %u bytes\n", 
               sections[i].name, sections[i].id, sections[i].ptr);
    }
    printf(" -> Symbols:    %d\n", symbol_count);
    printf(" -> Relocs:     %d\n", reloc_count);

    return 0;
}