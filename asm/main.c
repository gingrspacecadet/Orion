#include <stdlib.h>
#include <stdio.h>
#include "codegen.h"

static char *load_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); exit(1); }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { perror("ftell"); exit(1); }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { perror("malloc"); exit(1); }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { perror("fread"); exit(1); }
    fclose(f);
    *out_len = (size_t)sz;
    buf[sz] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s file.asm\n", argv[0]);
        return 1;
    }

    size_t flen = 0;
    char *data = load_file(argv[1], &flen);

    Source source = {
        .data = data,
        .line = 1,
        .offset = 0,
    };

    FILE *out = fopen("a.out", "wb");
    codegen(out, &source);
}