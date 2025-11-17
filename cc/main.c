#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void parse(char* line) {

}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Incorrect usage:\n"
        "  cc <in.c>\n");
        return EXIT_FAILURE;
    }


    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    char *string = malloc(fsize + 1);
    fread(string, fsize, 1, f);
    fclose(f);

    string[fsize] = 0;

    char* line = strtok(string, "\n");
    while (line) {
        parse(line);
        line = strtok(NULL, "\n");
    }

    free(string);

    return EXIT_SUCCESS;
}