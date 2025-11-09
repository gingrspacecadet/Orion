#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>

typedef struct {
    char* name;
    uint32_t offset;
} Label;

static Label labels[64] = {0};
static size_t num_labels = 0;
static uint32_t offset_counter = 0;

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
    { .name = "NOP", .type = R,  .opcode = 0b00000000, .num_operands = 0},
    { .name = "MOV", .type = RI, .opcode = 0b00000100, .num_operands = 2},
    { .name = "ADD", .type = RI, .opcode = 0b00001000, .num_operands = 2},
    { .name = "SUB", .type = RI, .opcode = 0b00001100, .num_operands = 2},
    { .name = "AND", .type = RI, .opcode = 0b00010000, .num_operands = 2},
    { .name = "OR",  .type = RI, .opcode = 0b00010100, .num_operands = 2},
    { .name = "XOR", .type = RI, .opcode = 0b00011000, .num_operands = 2},
    { .name = "LSL", .type = RI, .opcode = 0b00011100, .num_operands = 2},
    { .name = "LSR", .type = RI, .opcode = 0b00100000, .num_operands = 2},
    { .name = "ASR", .type = RI, .opcode = 0b00100100, .num_operands = 2},
    { .name = "LDR", .type = I,  .opcode = 0b00101000, .num_operands = 2},
    { .name = "STR", .type = I,  .opcode = 0b00101100, .num_operands = 2},
    { .name = "CMP", .type = RI, .opcode = 0b00110000, .num_operands = 2},
    { .name = "B",   .type = M,  .opcode = 0b00110100, .num_operands = 2},
    { .name = "BL",  .type = M,  .opcode = 0b00111000, .num_operands = 2},
    { .name = "BEQ", .type = M,  .opcode = 0b00111100, .num_operands = 2},
    { .name = "BNE", .type = M,  .opcode = 0b01000000, .num_operands = 2},
    { .name = "BCS", .type = M,  .opcode = 0b01000100, .num_operands = 2},
    { .name = "BCC", .type = M,  .opcode = 0b01001000, .num_operands = 2},
    { .name = "BGT", .type = M,  .opcode = 0b01001100, .num_operands = 2},
    { .name = "BLT", .type = M,  .opcode = 0b01010000, .num_operands = 2},
    { .name = "PUSH",.type = I,  .opcode = 0b01010100, .num_operands = 2},
    { .name = "POP", .type = I,  .opcode = 0b01011000, .num_operands = 2},
    { .name = "HALT",.type = R,  .opcode = 0b01011100, .num_operands = 0},
    { .name = "ADR", .type = I,  .opcode = 0b01100000, .num_operands = 2},
    { .name = "CMPU",.type = RI, .opcode = 0b01100100, .num_operands = 2},
    { .name = "MUL", .type = RI, .opcode = 0b01101000, .num_operands = 2},
    { .name = "DIV", .type = RI, .opcode = 0b01101100, .num_operands = 2},
    { .name = "LDRB",.type = I,  .opcode = 0b01110000, .num_operands = 2},
    { .name = "STRB",.type = I,  .opcode = 0b01110100, .num_operands = 2},
    { .name = "TEST",.type = RI, .opcode = 0b01111000, .num_operands = 2},
    { .name = "SVC", .type = I,  .opcode = 0b01111100, .num_operands = 2},
};

static void trim_inplace(char *s) {
    if (!s) return;
    // left trim
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    // right trim
    size_t len = strlen(s);
    while (len && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

static int parse_register(const char *word) {
    if (!word) return -1;
    if (toupper((unsigned char)word[0]) != 'R') return -1;
    char *endptr = NULL;
    long val = strtol(word + 1, &endptr, 10);
    if (endptr == word + 1 || *endptr != '\0') return -1;
    if (val < 0 || val > 31) return -1; // allow up to R31 if desired
    return (int)val;
}

static int parse_immediate(const char *word, uint32_t *out) {
    if (!word || !out) return -1;
    if (word[0] != '#') return -1;
    errno = 0;
    long val = strtol(word + 1, NULL, 0); // base 0 allows 0x prefixed hex
    if (errno) return -1;
    *out = (uint32_t)val;
    return 0;
}

void parse_line(char* line, uint32_t** out_words) {
    if (!line || !out_words || !*out_words) return;

    char buf[1024];
    strncpy(buf, line, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    trim_inplace(buf);
    if (buf[0] == '\0') return;

    // tokenise: allow commas and spaces as separators
    char *saveptr = NULL;
    char *tok = strtok_r(buf, " \t", &saveptr);
    if (!tok) return;

    // check label
    char *colon = strchr(tok, ':');
    if (colon) {
        *colon = '\0';
        if (num_labels >= sizeof(labels)/sizeof(labels[0])) {
            fprintf(stderr, "Too many labels\n");
            return;
        }
        labels[num_labels++].name = strdup(tok);
        labels[num_labels-1].offset = offset_counter;
        return;
    }

    // find opcode
    int opcode_idx = -1;
    for (int i = 0; i < (int)(sizeof(opcodes)/sizeof(opcodes[0])); i++) {
        if (!opcodes[i].name) continue;
        if (strcasecmp(opcodes[i].name, tok) == 0) { opcode_idx = i; break; }
    }
    if (opcode_idx < 0) {
        fprintf(stderr, "Unknown opcode: %s\n", tok);
        return;
    }

    Opcode *op = &opcodes[opcode_idx];

    // collect operands up to 3 tokens; treat comma separated in same token
    char *operands[4] = {0};
    size_t opcount = 0;
    while (opcount < op->num_operands) {
        char *next = strtok_r(NULL, ", \t", &saveptr);
        if (!next) break;
        trim_inplace(next);
        if (next[0] == '\0') continue;
        operands[opcount++] = strdup(next);
    }

    if (opcount < op->num_operands) {
        fprintf(stderr, "Not enough operands for %s\n", op->name);
        for (size_t k = 0; k < opcount; k++) free(operands[k]);
        return;
    }

    uint32_t encoded = 0;
    // layout (example): [31:24]=opcode (8 bits), [23:20]=Rd (4 bits), [19:16]=Rn (4 bits),
    // bit0 used as imm flag if needed, and [15:0] immediate when used.
    // opcode placed at bits 31..24
    encoded = ((uint32_t)op->opcode) << 24;

    switch (op->type) {
    case R:
    case I: {
        if (op->num_operands >= 1) {
            int rdst = parse_register(operands[0]);
            if (rdst < 0) { fprintf(stderr, "Invalid destination register: %s\n", operands[0]); goto cleanup; }
            encoded |= ((uint32_t)rdst & 0xF) << 20; // Rd in bits 23..20
        }
        if (op->num_operands >= 2) {
            int rsrc = parse_register(operands[1]);
            if (rsrc < 0) { fprintf(stderr, "Invalid source register: %s\n", operands[1]); goto cleanup; }
            encoded |= ((uint32_t)rsrc & 0xF) << 16; // Rn in bits 19..16
        }
        break;
    }
    case RI: {
        // operand0 = destination register; operand1 = register or immediate (#...)
        int rdst = parse_register(operands[0]);
        if (rdst < 0) { fprintf(stderr, "Invalid destination register: %s\n", operands[0]); goto cleanup; }
        encoded |= ((uint32_t)rdst & 0xF) << 20;

        if (toupper((unsigned char)operands[1][0]) == 'R') {
            int rsrc = parse_register(operands[1]);
            if (rsrc < 0) { fprintf(stderr, "Invalid source register: %s\n", operands[1]); goto cleanup; }
            encoded |= ((uint32_t)rsrc & 0xF) << 16; // put register in Rn field
            // imm flag = 0 -> bit 0 already clear
        } else if (operands[1][0] == '#') {
            uint32_t imm = 0;
            if (parse_immediate(operands[1], &imm) != 0) { fprintf(stderr, "Invalid immediate: %s\n", operands[1]); goto cleanup; }
            // place immediate into lower 16 bits
            encoded |= (imm & 0xFFFF);
            // set low bit to indicate immediate (as in your original code)
            encoded |= 1U;
        } else {
            fprintf(stderr, "Missing register or immediate: %s\n", operands[1]);
            goto cleanup;
        }
        break;
    }
    case M:
        // Branch / memory-meta type: placeholder — store opcode and operands raw for now
        if (op->num_operands >= 1) {
            int rdst = parse_register(operands[0]);
            if (rdst >= 0) encoded |= ((uint32_t)rdst & 0xF) << 20;
        }
        if (op->num_operands >= 2) {
            // second operand might be label; we simply copy a small hash or leave for later backpatch
            // For now, if it's immediate, parse; if label, leave immediate 0.
            if (operands[1][0] == '#') {
                uint32_t imm = 0;
                if (parse_immediate(operands[1], &imm) == 0) encoded |= (imm & 0xFFFF);
            } else {
                // label: we don't resolve here; could store index or leave 0
            }
        }
        break;
    default:
        break;
    }

    // write encoded word
    **out_words = encoded;
    (*out_words)++;
    offset_counter++;

cleanup:
    for (size_t k = 0; k < opcount; k++) free(operands[k]);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.asm out.bin\n", argv[0]);
        return 1;
    }

    FILE* src = fopen(argv[1], "r");
    if (!src) { perror("fopen input"); return 1; }
    FILE* dest = fopen(argv[2], "wb");
    if (!dest) { perror("fopen output"); fclose(src); return 1; }

    size_t capacity = 4096;
    uint32_t *base_out = calloc(capacity, sizeof(uint32_t));
    if (!base_out) { perror("calloc"); fclose(src); fclose(dest); return 1; }
    uint32_t *out = base_out;

    char linebuf[1024];
    while (fgets(linebuf, sizeof(linebuf), src)) {
        // remove newline
        char *nl = strchr(linebuf, '\n');
        if (nl) *nl = '\0';
        // skip empty or comment lines (lines starting with ';' or '#')
        char tmp[1024];
        strncpy(tmp, linebuf, sizeof(tmp)-1);
        tmp[sizeof(tmp)-1] = '\0';
        trim_inplace(tmp);
        if (tmp[0] == '\0') continue;
        if (tmp[0] == ';' || tmp[0] == '#') continue;

        // expand output buffer if needed
        if ((size_t)(out - base_out) + 1 >= capacity) {
            size_t newcap = capacity * 2;
            uint32_t *nptr = realloc(base_out, newcap * sizeof(uint32_t));
            if (!nptr) { perror("realloc"); break; }
            out = nptr + (out - base_out);
            base_out = nptr;
            capacity = newcap;
        }
        parse_line(linebuf, &out);
    }

    size_t words_written = (size_t)(out - base_out);
    if (words_written > 0) {
        if (fwrite(base_out, sizeof(uint32_t), words_written, dest) != words_written) {
            perror("fwrite");
        }
    }

    // cleanup
    for (size_t i = 0; i < num_labels; i++) {
        free(labels[i].name);
    }
    free(base_out);
    fclose(src);
    fclose(dest);

    return 0;
}
