#include "isa.h"
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static uint8_t *load_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); exit(1); }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { perror("ftell"); exit(1); }
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { perror("malloc"); exit(1); }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { perror("fread"); exit(1); }
    fclose(f);
    *out_len = (size_t)sz;
    return buf;
}

static uint32_t load_le32(const uint8_t b[4]) {
    return ((uint32_t)b[0]) |
           ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s out.bin\n", argv[0]);
        return 1;
    }

    size_t imglen;
    uint8_t *img = load_file(argv[1], &imglen);

    size_t i = 0;
    while (i < imglen) {
        uint32_t raw = load_le32(&img[i]);

        if (raw == 0) {
            size_t zeros = 0;
            while (i < imglen && load_le32(&img[i]) == 0) {
                zeros++;
                i += 4; 
            }
            printf("(%zu zero words)\n", zeros);
            continue;
        }


        char buffer[1024];
        isa_disassemble(buffer, 1024, i, raw);
        printf(buffer);
        
        printf("\n");
        i += 4;
    }
}