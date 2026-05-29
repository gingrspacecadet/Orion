#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "lexer.h"
#include "parser.h"
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
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.s> <output.o>\n", argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];

    char *source = read_file(input_path);

    token_array tokens = lex(source);
    free(source);

    instr_array instrs = parse(&tokens);
    token_array_free(&tokens); 

    codegen(&instrs);
    instr_array_free(&instrs);
    
    FILE *out = fopen(output_path, "wb");
    if (!out) {
        perror("Error creating output object file");
        return 1;
    }

    ObjHeader header = {
        .magic = OBJ_MAGIC,
        .text_size = text_ptr,
        .data_size = data_ptr,
        .sym_count = (uint32_t)symbol_count,
        .reloc_count = (uint32_t)reloc_count
    };

    fwrite(&header, sizeof(ObjHeader), 1, out);

    if (text_ptr > 0) {
        fwrite(text_section, 1, text_ptr, out);
    }
    if (data_ptr > 0) {
        fwrite(data_section, 1, data_ptr, out);
    }

    if (symbol_count > 0) {
        fwrite(symbol_table, sizeof(ObjSymbol), symbol_count, out);
    }
    if (reloc_count > 0) {
        fwrite(reloc_table, sizeof(ObjReloc), reloc_count, out);
    }

    fclose(out);
    
    printf("Wrote output to %s\n", output_path);
    printf(" -> .text size: %u bytes\n", text_ptr);
    printf(" -> .data size: %u bytes\n", data_ptr);
    printf(" -> Symbols:    %d\n", symbol_count);
    printf(" -> Relocs:     %d\n", reloc_count);

    return 0;
}