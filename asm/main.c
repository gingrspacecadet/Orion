#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <strings.h>
#include <stdarg.h>

#include "ops.h"

typedef struct {
    char* name;
    uint32_t offset;
} Label;

static Label labels[64] = {0};
static size_t num_labels = 0;
static uint32_t offset = 0;

char* error;

#define ANSI_RED     "\x1b[31m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BOLD    "\x1b[1m"
#define ANSI_RESET   "\x1b[0m"

static void set_error(const char *fmt, ...) {
    if (!error) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error, 1024, fmt, ap);
    va_end(ap);
}

static void set_error_pretty(const char *fmt, ...) {
    if (!error) return;
    va_list ap;
    va_start(ap, fmt);
    char body[900];
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);
    snprintf(error, 1024, ANSI_BOLD ANSI_RED "Error:" ANSI_RESET " %s", body);
}

int parse_reg(const char *str) {
    if (!str) {
        set_error_pretty("Missing register token; expected form like %s.", "R3");
        return 1;
    }

    if (toupper((unsigned char)str[0]) != 'R') {
        set_error_pretty("Invalid register '%s'. Registers must start with %s.", str, "R");
        return 1;
    }

    const char *p = str + 1;
    if (*p == '\0') {
        set_error_pretty("Empty register index in '%s'. Example valid token: %s.", str, "R0");
        return 1;
    }
    for (; *p; ++p) {
        if (!isdigit((unsigned char)*p)) {
            set_error_pretty("Invalid character '%c' in register '%s'. Only decimal digits allowed after 'R'.",
                             *p, str);
            return 1;
        }
    }

    long val = strtol(str + 1, NULL, 10);
    if (val < 0 || val > 15) {
        set_error_pretty("Register index out of range in '%s'. Allowed: 0..%ld. Example: R%ld.",
                         str, 15, 15);
        return 1;
    }

    return 0;
}

Label* parse_label(const char *str) {
    if (!str) {
        set_error_pretty("Missing label token; expected something like %s.", "$loop");
        return NULL;
    }
    if (str[0] != '$') {
        set_error_pretty("Invalid label '%s'. Labels must start with '%s'. Example: %s", str, "$", "$start");
        return NULL;
    }
    const char *name = str + 1;
    if (*name == '\0') {
        set_error_pretty("Empty label name in token '%s'. Provide a name after '$', e.g. %s.", str, "$handler");
        return NULL;
    }

    for (size_t i = 0; i < num_labels; ++i) {
        if (labels[i].name && strcmp(labels[i].name, name) == 0) {
            return &labels[i];
        }
    }

    set_error_pretty("Unknown label '%s'. Did you forget to define it? Available labels:", name);
    // append a short list of known labels (non-exhaustive if many)
    {
        size_t shown = 0;
        size_t max_show = 6;
        char listbuf[512] = {0};
        for (size_t i = 0; i < num_labels && shown < max_show; ++i) {
            if (labels[i].name) {
                if (shown) strncat(listbuf, ", ", sizeof(listbuf));
                strncat(listbuf, labels[i].name, sizeof(listbuf));
                shown++;
            }
        }
        if (shown > 0) {
            size_t cur_len = strlen(error);
            snprintf(error + cur_len, 1024 - cur_len, " %s", listbuf);
        } else {
            size_t cur_len = strlen(error);
            snprintf(error + cur_len, 1024 - cur_len, " (no labels defined)");
        }
    }

    return NULL;
}

int parse_imm(char *str) {
    if (!str) {
        set_error_pretty("Missing immediate token; expected a literal like %s or %s.", "#42", "#0x2A");
        return 1;
    }
    if (str[0] == '$') {
        sprintf(str + 1, "%ld", parse_label(str)->offset);
        *str = '#';
        return parse_imm(str);
    }
    if (str[0] != '#') {
        set_error_pretty("Immediate '%s' must start with '#'. Example: %s or %s.", str, "#10", "#-5");
        return 1;
    }

    const char *p = str + 1;
    if (*p == '\0') {
        set_error_pretty("Immediate '%s' contains no digits. Examples: %s, %s.", str, "#10", "#0xFF");
        return 1;
    }

    if (*p == '+' || *p == '-') ++p;
    if (*p == '\0') {
        set_error_pretty("Immediate '%s' has only a sign but no digits.", str);
        return 1;
    }

    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        if (*p == '\0') {
            set_error_pretty("Immediate '%s' has '0x' with no hex digits.", str);
            return 1;
        }
        for (; *p; ++p) {
            if (!isxdigit((unsigned char)*p)) {
                set_error_pretty("Invalid hex digit '%c' in immediate '%s'. Expected 0-9 or A-F.", *p, str);
                return 1;
            }
        }
    } else {
        for (; *p; ++p) {
            if (!isdigit((unsigned char)*p)) {
                set_error_pretty("Invalid decimal digit '%c' in immediate '%s'.", *p, str);
                return 1;
            }
        }
    }

    return 0;
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

char *trimwhitespace(char *str) {
    char *end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

void parse(char* line, uint32_t** out) {
    line = trimwhitespace(line);
    // puts(line);
    char* comment = strchr(line, ';');
    if (line == comment) { (*out)--; offset--; return; }
    if (*line == '\n') { (*out)--; offset--; return; }
    if (comment) *comment = '\0';
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
    char* arg3 = strtok(NULL, " ");
    if (arg1 && strchr(arg1, ',') != NULL) *strchr(arg1, ',') = '\0';
    if (arg2 && strchr(arg2, ',') != NULL) *strchr(arg2, ',') = '\0';

    switch(opcodes[index].type) {
    case I:
        printf("Writing opcode %s with operands '%s', '%s' and '%s'\n", word1, arg1, arg2, arg3);
        **out = ((uint32_t)opcodes[index].opcode << (24));
        
        **out |= (uint32_t)atoi(arg1 + 1) << (32 - 6 - 4);

        if (parse_reg(arg1) == 0 && parse_reg(arg2) == 0 && parse_imm(arg3) == 0) {
            if (opcodes[index].num_operands > 0) {
                **out |= (uint32_t)atoi(arg1 + 1) << (32 - 6 - 4);
                **out |= (uint32_t)atoi(arg2 + 1) << (32 - 6 - 4 - 4);
                **out |= (int32_t)strtol(arg3 + 1, NULL, 0) << (32 - 6 - 4 - 4 - 16); 
            }
        } if (parse_imm(arg1) == 0) {
            **out |= (int32_t)strtol(arg1 + 1, NULL, 0) << (32 - 6 - 4 - 4 - 16); 
        }
        
        printf("OUT: 0b%032b\n", **out);
        **out = **out;
         
        break;
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
        printf("Writing opcode %s with operands '%s', '%s' and '%s'\n", word1, arg1, arg2, arg3);
        if (parse_imm(arg2) == 0) {
            **out = ((uint32_t)opcodes[index].opcode << (24));
            
            **out |= (uint32_t)atoi(arg1 + 1) << (32 - 6 - 4);

            if (opcodes[index].num_operands > 0) {
                **out |= (uint32_t)atoi(arg1 + 1) << (32 - 6 - 4);
                **out |= (int32_t)strtol(arg2 + 1, NULL, 0) << (32 - 6 - 4 - 4 - 16); 
            }
            **out |= 1;
        } else if (parse_imm(arg3) == 0) {
            **out = ((uint32_t)opcodes[index].opcode << (24));
            
            **out |= (uint32_t)atoi(arg1 + 1) << (32 - 6 - 4);

            if (opcodes[index].num_operands > 0) {
                **out |= (uint32_t)atoi(arg1 + 1) << (32 - 6 - 4);
                **out |= (uint32_t)atoi(arg2 + 1) << (32 - 6 - 4 - 4);
                **out |= (int32_t)strtol(arg3 + 1, NULL, 0) << (32 - 6 - 4 - 4 - 16); 
            }
            **out |= 1;
        } else {
            if (opcodes[index].num_operands > 0) {
                if (parse_reg(arg1) != 0) {
                    puts(error);
                    exit(1);
                } else if (parse_reg(arg2) != 0) {
                    puts(error);
                    exit(1);
                }
            }
            
            **out = ((uint32_t)opcodes[index].opcode << (24));
            
            if (opcodes[index].num_operands > 0) {
                **out |= (uint32_t)atoi(arg1 + 1) << (32 - 6 - 4);
                **out |= (uint32_t)atoi(arg2 + 1) << (32 - 6 - 4 - 4);
            }
        }
        
        
        printf("OUT: 0b%032b\n", **out);
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
        char* comment = strchr(word1, ';');
        if (colon != NULL) {    // LABEL
            *colon = '\0';
            labels[num_labels++] = (Label){ .name = strdup(word1), .offset = offset};
        } else if (comment != word1 && *word1 != '\n') {
            offset++;
        }
    }
    rewind(src);
    offset = 0;
    while(fgets(buf, 1024, src)) {
        if (*buf == '\n') continue;
        if (strchr(buf, '\n')) *strchr(buf, '\n') = '\0';
        parse(buf, &out);
        out++;
    }

    fwrite(base_out, sizeof(uint32_t), offset, dest);

    for (size_t i = 0; i < num_labels; i++) {
        printf("Found label %s with offset 0x%04X\n", labels[i].name, labels[i].offset);
        free(labels[i].name);
    }

    return 0;
}