#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <strings.h>

#include "ops.h"

typedef struct {
    char* name;
    uint32_t offset;
} Label;

static Label labels[64] = {0};
static size_t num_labels = 0;
static uint32_t offset = 0;

char* error;

int parse_reg(char* str) {
    if (toupper(*str) != 'R') {
        strcpy(error, "Not a valid register. Registers start with [rR].");
        return 1;
    }
    if (atoi(str + 1) > 16) {
        strcpy(error, "Register count exceeded. Maximum register index is 16");
        return 1;
    }
    return 0;
}

int parse_imm(char* str) {
    if (*str != '#') {
        strcpy(error, "Literals begin with '#'.");
        return 1;
    }
    bool negative = false;
    if (str[1] == '-') {
        negative = true;
    }
    if (toupper(str[2]) == 'X') {
        for (size_t i = 3 + negative; i < strlen(str + 3 + negative); i++) {
            if (!isxdigit(str[i])) {
                strcpy(error, "Not a valid hex digit.");
                return 1;
            }
        }
    } else {
        for (size_t i = 1 + negative; i < strlen(str + 1 + negative); i++) {
            if (!isxdigit(str[i])) {
                strcpy(error, "Not a valid decimal digit.");
                return 1;
            }
        }
    }

    return 0;
}

Label* parse_label(char* str) {
    if (*str != '$') {
        strcpy(error, "Labels must start with a '$'");
        return NULL;
    }
    Label* label = NULL;
    for (size_t i = 0; i < num_labels; i++) {
        if (strcmp(labels[i].name, str+1) == 0) {
            label = &labels[i];
        }
    }
    if (!label) {
        strcpy(error, "Label does not exist");
        return NULL;
    }

    return label;
}

uint8_t parse_opcode(char* str) {
    uint8_t index = 0xFF;
    for (int i = 0; i < 64; i++) {
        if (strcasecmp(opcodes[i].name, str) == 0) {
            index = i;
            break;
        }
    }
    if (index == 0xFF) {
        printf("Invalid opcode %s", str);
    }
    return index;
}

void parse(char* line, uint32_t** out) {
    char* word1 = strtok(line, " ");
    char* colon = strchr(word1, ':');
    if (colon != NULL) {    // LABEL
        --*out;
        return;
    }
    // NOT A LABEL
    uint8_t index = parse_opcode(word1);
    char* arg1 = strtok(NULL, " ");
    char* arg2 = strtok(NULL, " ");

    switch(opcodes[index].type) {
    case I: break;
    case R:
        if (opcodes[index].num_operands > 0) {
            if (parse_reg(arg1) != 0) {
                puts(error);
                exit(1);
            } else if (parse_reg(arg2) != 0) {
                puts(error);
                exit(1);
            }
        }
        
        printf("Writing opcode %s with operands %s and %s\n", word1, arg1, arg2);
        **out = ((uint32_t)opcodes[index].opcode << (24));
        
        if (opcodes[index].num_operands > 0) {
            **out |= (uint32_t)atoi(arg1 + 1) << (32 - 6 - 4);
            **out |= (uint32_t)atoi(arg2 + 1) << (32 - 6 - 4 - 4);
        }
        
        printf("OUT: 0b%032b\n", **out);
        break;
    case M:
        printf("Writing opcode %s with operands %s and %s\n", word1, arg1, arg2);
        if (opcodes[index].num_operands > 0) {
            **out = ((uint32_t)opcodes[index].opcode << (24));
            if (parse_imm(arg1) == 0) {
                if (opcodes[index].num_operands > 0) {
                    **out |= ((int32_t)strtol(arg1 + 1, NULL, 0) << (32 - 6 - 24) & 0b11111111111111111111111111);
                }
            } else if (parse_label(arg1) != NULL) {    
                printf("%s (%d) - %d = %032B\n", parse_label(arg1)->name, parse_label(arg1)->offset, offset, (parse_label(arg1)->offset - offset) & 0b00000011111111111111111111111100);
                if (opcodes[index].num_operands > 0) {
                    **out |= ((parse_label(arg1)->offset - offset) << 2 & 0b00000011111111111111111111111100);
                }
            } else {
                puts(error);
                exit(1);
            }
        }
        
        
        printf("OUT: 0b%032b\n", **out);
        
        break;
    case RI:
        
        printf("Writing opcode %s with operands %s and %s\n", word1, arg1, arg2);
        uint32_t out_tmp = ((uint32_t)opcodes[index].opcode << (24));
        
        out_tmp |= (uint32_t)atoi(arg1 + 1) << (32 - 6 - 4);
        if (parse_reg(arg2) == 0) {
            out_tmp |= (uint32_t)atoi(arg2 + 1) << (32 - 6 - 4 - 4);
        } else if (parse_imm(arg2) == 0) {
            out_tmp |= (int32_t)strtol(arg2 + 1, NULL, 0) << (32 - 6 - 4 - 4 - 16);
            out_tmp |= 1;
        } else {
            puts(error);
            exit(1);
        }
        
        printf("OUT: 0b%032b\n", out_tmp);
        **out = out_tmp;
        
        break;
    default:
    }

    offset++;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        return 1;
    }

    error = malloc(sizeof(char) * 1024);
    
    FILE* src = fopen(argv[1], "r");
    FILE* dest = fopen(argv[2], "wb");
    
    uint32_t* base_out = calloc(1024, sizeof(uint32_t));
    uint32_t* out = base_out;
    
    char buf[1024];
    while(fgets(buf, 1024, src)) {
        char* word1 = strtok(buf, " ");
        char* colon = strchr(word1, ':');
        if (colon != NULL) {    // LABEL
            *colon = '\0';
            labels[num_labels++] = (Label){ .name = strdup(word1), .offset = offset};
        } else offset++;
    }
    rewind(src);
    offset = 0;
    while(fgets(buf, 1024, src)) {
        if (*buf == '\n') continue;
        if (strchr(buf, '\n')) *strchr(buf, '\n') = '\0';
        parse(buf, &out);
        out++;
    }

    size_t out_size;
    for (out_size = 0; base_out[out_size] != NULL; out_size++);

    fwrite(base_out, sizeof(uint32_t), out_size, dest);

    for (size_t i = 0; i < num_labels; i++) {
        printf("Found label %s with offset 0x%04X\n", labels[i].name, labels[i].offset);
        free(labels[i].name);
    }

    return 0;
}