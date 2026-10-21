#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "isa.h"

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

#define X(NAME) #NAME,
const char *const opcode_names[] = {
    X_OPCODES
};
#undef X

const struct {Opcodes code; OpcodeTypes type;}
opcodes[] = {
    [OP_ADD] = {OP_ADD, TYPE_A},
    [OP_NOP] = {OP_NOP, TYPE_X},
    [OP_LDR] = {OP_LDR, TYPE_M},
    [OP_LUI] = {OP_LUI, TYPE_A},
};

static void print_instr(uint32_t word) {
    printf("%s ", opcode_names[OPCODE(word)]);
    switch (opcodes[OPCODE(word)].type) {
        case TYPE_A:
        case TYPE_M: {
            printf("r%u, r%u, ", A_RD(word), A_RN(word));
            if (A_REGISTER(word))
                printf("r%u", A_OPERAND(word) >> 11);
            else if ((int16_t)A_OPERAND(word) < 0)
                printf("#%u (%d)", A_OPERAND(word), (int16_t)A_OPERAND(word));
            else
                printf("#%u", A_OPERAND(word));
                
            break;
        }

        case TYPE_X: {
            break;
        }

        default: {
            break;
        }
    }

    putchar('\n');
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s file.bin", argv[0]);
        return 1;
    }

    size_t len;
    char *data = load_file(argv[1], &len);

    for (size_t i = 0; i < len; i += 4) {
        uint32_t word;
        memcpy(&word, data + i, sizeof(word));
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        x = __builtin_bswap32(x);
#endif

        print_instr(word);
    }

    return 0;
}