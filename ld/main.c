#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "linker.h"

uint32_t text_vma = 0x00020000;
uint32_t data_vma = 0x00030000;

uint8_t exec_text[MAX_EXEC_SIZE];
uint32_t exec_text_size = 0;

uint8_t exec_data[MAX_EXEC_SIZE];
uint32_t exec_data_size = 0;

GlobalSymbol global_sym_table[MAX_GLOBAL_SYMBOLS];
int global_sym_count = 0;

DeferredReloc master_reloc_table[MAX_TOTAL_RELOCS];
int master_reloc_count = 0;

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [-Ttext addr] [-Tdata addr] [-o out.bin] <file1.o> [file2.o ...]\n", argv[0]);
        return 1;
    }

    const char *output_path = "out.bin";

    const char **input_files = malloc(sizeof(char *) * argc);
    int input_file_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-Ttext") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Linker Error: -Ttext requires a target address argument.\n");
                return 1;
            }
            text_vma = strtoul(argv[i + 1], NULL, 0);
            i++;
        }
        else if (strcmp(argv[i], "-Tdata") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Linker Error: -Tdata requires a target address argument.\n");
                return 1;
            }
            data_vma = strtoul(argv[i + 1], NULL, 0);
            i++;
        }
        else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Linker Error: -o requires an output filename.\n");
                return 1;
            }
            output_path = argv[i + 1];
            i++;
        }
        else {
            input_files[input_file_count++] = argv[i];
        }
    }

    if (input_file_count == 0) {
        fprintf(stderr, "Linker Error: No input object files specified.\n");
        return 1;
    }

    printf(" -> Target .text VMA: 0x%08X\n", text_vma);
    printf(" -> Target .data VMA: 0x%08X\n", data_vma);

    for (int i = 0; i < input_file_count; i++)
        link_object_file(input_files[i]);
    resolve_relocations();
    write_executable(output_path);

    return 0;
}