#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

typedef struct {
    char* name;
    uint32_t offset;
} Label;

static Label labels[64] = {0};
static size_t num_labels = 0;
static uint32_t offset = 0;

typedef struct {
    char* name;
    enum {
        I,
        R,
        RI,
        M,
    } type;
    uint8_t opcode;
    size_t num_operands;
} Opcode;

static Opcode opcodes[64] = {
    [55] = { .name = "MOV", .type = RI, .opcode = (uint8_t)0b00000100 , .num_operands = 2},
    [46] = { .name = "HALT", .type = R, .opcode = (uint8_t)0b01011100 , .num_operands = 0}
};

static uint64_t hash(unsigned char* str) {
    uint64_t hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; // hash * 33 + c

    return hash;
}

#define validate_reg(word) ({ \
    *strchr(word, ',') = '\0'; \
    if (toupper(*word) != 'R') return; \
    if (!isdigit(word[1])) return; \
    if (strlen(word) > 2 && !isdigit(word[2])) return; \
}) \

void parse(char* line, uint32_t** out) {
    char* word1 = strtok(line, " ");
    char* colon = strchr(word1, ':');
    if (colon != NULL) {    // LABEL
        *colon = '\0';
        labels[num_labels++] = (Label){ .name = strdup(word1), .offset = offset};
        return;
    } else {        // NOT A LABEL
        for (size_t i = 0; i < strlen(word1); i++) word1[i] = toupper(word1[i]);
        uint64_t hashed = hash(word1) % 64;
        char* word2 = strtok(NULL, " ");
        char* word3 = strtok(NULL, " ");
        
        if (opcodes[hashed].num_operands > 0) validate_reg(word2);

        if (opcodes[hashed].name) {
            switch(opcodes[hashed].type) {
            case I:
            case R:
                if (opcodes[hashed].num_operands > 0) validate_reg(word3);

                printf("Writing opcode %s with operands %s and %s\n", word1, word2, word3);
                **out = ((uint32_t)opcodes[hashed].opcode << (24));

                if (opcodes[hashed].num_operands > 0) {
                    **out |= (uint32_t)atoi(word2 + 1) << (32 - 6 - 4);
                    **out |= (uint32_t)atoi(word3 + 1) << (32 - 6 - 4 - 4);
                }

                printf("OUT: 0b%032b\n", **out);

                break;
            case M: break;
            case RI:
                
                bool is_reg = false;
                // word3 should be a register or a number in the form #[0-9a-fA-F]*
                if (*word3 == '#') {
                    for (size_t i = 1; i < strlen(word3); i++) {
                        if (!isxdigit(word3[i])) return;
                    }
                } else if (toupper(*word3) == 'R') {
                    is_reg = true;
                } else return;

                printf("Writing opcode %s with operands %s and %s\n", word1, word2, word3);
                uint32_t out_tmp = ((uint32_t)opcodes[hashed].opcode << (24));

                out_tmp |= (uint32_t)atoi(word2 + 1) << (32 - 6 - 4);
                if (is_reg) {
                    out_tmp |= (uint32_t)atoi(word3 + 1) << (32 - 6 - 4 - 4);
                } else {
                    out_tmp |= (uint32_t)strtol(word3 + 1, NULL, 0) << (32 - 6 - 4 - 4 - 16);
                    out_tmp |= 1;
                }

                printf("OUT: 0b%032b\n", out_tmp);
                **out = out_tmp;

                break;
            default:
            }
        }
    }

    offset++;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        return 1;
    }
    
    FILE* src = fopen(argv[1], "r");
    FILE* dest = fopen(argv[2], "wb");
    
    uint32_t* base_out = calloc(1024, sizeof(uint32_t));
    uint32_t* out = base_out;
    
    char buf[1024];
    while(fgets(buf, 1024, src)) {
        if (*buf == '\n') continue;
        if (strchr(buf, '\n')) *strchr(buf, '\n') = '\0';
        printf("Parsing %s...\n", buf);
        parse(buf, &out);
        out++;
    }

    fwrite(base_out, sizeof(uint32_t), 1024, dest);

    for (size_t i = 0; i < num_labels; i++) {
        printf("Found label %s at offset 0x%04X\n", labels[i].name, labels[i].offset);
        free(labels[i].name);
    }

    return 0;
}