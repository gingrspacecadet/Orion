#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "linker.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [-T script.ld] [-o out.bin] <file1.o> [file2.o ...]\n", argv[0]);
        return 1;
    }

    const char *output_path = "out.bin";
    const char *script_path = NULL;

    const char **input_files = malloc(sizeof(char *) * argc);
    int input_file_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-T") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Linker Error: -T requires a linker script layout file path.\n");
                free(input_files);
                return 1;
            }
            script_path = argv[i + 1];
            i++;
        }
        else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Linker Error: -o requires an output filename binary path.\n");
                free(input_files);
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
        fprintf(stderr, "Linker Error: No input relocatable object (.o) files specified.\n");
        free(input_files);
        return 1;
    }

    if (!script_path) {
        fprintf(stderr, "Linker Error: Dynamic mapping requires a script. Please specify one with -T <script.ld>\n");
        free(input_files);
        return 1;
    }

    parse_linker_script(script_path);

    for (int i = 0; i < input_file_count; i++) {
        printf(" -> Linking: %s\n", input_files[i]);
        link_object_file(input_files[i]);
    }

    printf("Linker: Resolving global symbols and relocations...\n");
    resolve_relocations();

    printf("Linker: Emitting executable to '%s'...\n", output_path);
    write_executable(output_path);

    free(input_files);
    return 0;
}