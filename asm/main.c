#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

typedef enum {
    OP_NONE = 0,
    OP_REG,
    OP_LIT,
    OP_ADDR
} OperandKind;

typedef struct {
    uint8_t opcode;
    const char *name;
    uint8_t width;        /* total bytes including opcode */
    uint8_t n_operands;
    OperandKind ops[2];   /* max 2 operands in this ISA */
} OpEntry;

/* Unsorted table */
static OpEntry table[] = {
    {0x00, "NOP", 1, 0, {OP_NONE, OP_NONE}},

    {0x01, "ADD", 3, 2, {OP_REG, OP_REG}},
    {0x02, "SUB", 3, 2, {OP_REG, OP_REG}},
    {0x03, "MUL", 3, 2, {OP_REG, OP_REG}},
    {0x04, "DIV", 3, 2, {OP_REG, OP_REG}},
    {0x05, "MOD", 3, 2, {OP_REG, OP_REG}},

    {0x10, "LDI", 3, 2, {OP_REG, OP_LIT}},
    {0x11, "LDR", 3, 2, {OP_REG, OP_ADDR}},
    {0x12, "STR", 3, 2, {OP_ADDR, OP_REG}},
    {0x13, "MOV", 3, 2, {OP_REG, OP_REG}},

    {0x20, "CMP", 3, 2, {OP_REG, OP_REG}},
    {0x21, "JMP", 2, 1, {OP_ADDR, OP_NONE}},
    {0x22, "JZ",  2, 1, {OP_ADDR, OP_NONE}},
    {0x23, "JNZ", 2, 1, {OP_ADDR, OP_NONE}},

    {0x30, "PUSH",2, 1, {OP_REG, OP_NONE}},
    {0x31, "POP", 2, 1, {OP_REG, OP_NONE}},
    {0x32, "CALL",2, 1, {OP_ADDR, OP_NONE}},
    {0x33, "RET", 1, 0, {OP_NONE, OP_NONE}},

    {0x40, "IRET", 1, 0, {OP_NONE, OP_NONE}},

    {0xFF, "HLT", 1, 0, {OP_NONE, OP_NONE}},
};
static size_t table_len = sizeof(table) / sizeof(table[0]);

static int cmp_op(const void *a, const void *b) {
    const OpEntry *ea = a;
    const OpEntry *eb = b;
    return strcmp(ea->name, eb->name);
}

void init_ops(void) {
    qsort(table, table_len, sizeof(table[0]), cmp_op);
}

/* helpers to parse numeric forms */
static int parse_number(const char *s, uint8_t *out) {
    if (!s || !*s) return 0;
    char *end = NULL;
    unsigned long v = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        v = strtoul(s + 2, &end, 16);
    } else {
        v = strtoul(s, &end, 10);
    }
    if (end == s) return 0;
    if (v > 0xFF) return 0;
    *out = (uint8_t)v;
    return 1;
}

/* parse a register token like R3 -> 3 */
static int parse_register(const char *tok, uint8_t *out) {
    if (!tok || tok[0] != 'R') return 0;
    return parse_number(tok + 1, out);
}

/* parse a literal token like #10 or #0xFF */
static int parse_literal(const char *tok, uint8_t *out) {
    if (!tok || tok[0] != '#') return 0;
    return parse_number(tok + 1, out);
}

/* parse address token (numeric only) */
static int parse_addr_token(const char *tok, uint8_t *out) {
    return parse_literal(tok, out);
}

/* trim leading/trailing whitespace from a string in-place */
static void strtrim(char *s) {
    char *start = s;
    while (isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) *--end = '\0';
}

/* parse: tokenizes the line, looks up opcode with bsearch, writes to out[].
   Returns number of bytes written to out (0 on error). */
int parse(char *line, uint8_t *out, size_t outcap) {
    if (!line || !out) return 0;

    /* make a local copy and trim */
    char tmp[256];
    strncpy(tmp, line, sizeof(tmp));
    tmp[sizeof(tmp)-1] = '\0';
    strtrim(tmp);

    /* split mnemonic and rest */
    char *mn = strtok(tmp, " \t");
    if (!mn) return 0;

    /* make it uppercase */
    for (char *p = mn; *p; ++p) {
        *p = (char) toupper((unsigned char) *p);
    }

    OpEntry key = { .name = mn };
    OpEntry *found = bsearch(&key, table, table_len, sizeof(table[0]), cmp_op);
    if (!found) {
        fprintf(stderr, "Unknown opcode: %s\n", mn);
        return 0;
    }

    if (found->width > outcap) {
        fprintf(stderr, "Output buffer too small for %s\n", mn);
        return 0;
    }

    size_t write_i = 0;
    out[write_i++] = found->opcode;

    /* if no operands expected, we're done */
    if (found->n_operands == 0) {
        if (found->width != 1) {
            fprintf(stderr, "Internal width mismatch for %s\n", mn);
            return 0;
        }
        return 1;
    }

    /* get operand list string (rest of the original trimmed tmp after mnemonic) */
    char *rest = strtok(NULL, "");
    if (!rest) {
        fprintf(stderr, "Missing operands for %s\n", mn);
        return 0;
    }
    strtrim(rest);

    /* split operands by comma (max 2) */
    char *op_tok[2] = {0,0};
    char *p = rest;
    for (int idx = 0; idx < 2 && p && *p; ++idx) {
        while (isspace((unsigned char)*p)) p++;
        op_tok[idx] = p;
        /* find comma */
        char *comma = strchr(p, ',');
        if (comma) {
            *comma = '\0';
            p = comma + 1;
        } else {
            /* last token */
            p = NULL;
        }
    }

    /* validate operand count */
    if ((int)found->n_operands < 0 || found->n_operands > 2) return 0;
    for (uint8_t j = 0; j < found->n_operands; ++j) {
        if (!op_tok[j]) {
            fprintf(stderr, "Missing operand %u for %s\n", (unsigned)j+1, mn);
            return 0;
        }
        strtrim(op_tok[j]);
    }

    /* parse operands in order */
    for (uint8_t j = 0; j < found->n_operands; ++j) {
        uint8_t val = 0;
        OperandKind want = found->ops[j];
        int ok = 0;
        if (want == OP_REG) {
            ok = parse_register(op_tok[j], &val);
            if (!ok) fprintf(stderr, "Expected register for operand %u of %s\n", (unsigned)j+1, mn);
        } else if (want == OP_LIT) {
            ok = parse_literal(op_tok[j], &val);
            if (!ok) fprintf(stderr, "Expected literal (#N) for operand %u of %s\n", (unsigned)j+1, mn);
        } else if (want == OP_ADDR) {
            ok = parse_addr_token(op_tok[j], &val);
            if (!ok) fprintf(stderr, "Expected address (N or 0xNN) for operand %u of %s\n", (unsigned)j+1, mn);
        } else {
            fprintf(stderr, "Internal: unknown operand kind\n");
            return 0;
        }
        if (!ok) return 0;
        out[write_i++] = val;
    }

    /* sanity check width */
    if (write_i != found->width) {
        fprintf(stderr, "Width mismatch for %s: expected %u got %zu\n", mn, found->width, write_i);
        return 0;
    }

    return (int)write_i;
}

/* Small demo assembler: reads lines, writes bytes to argv[2] */
int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s source.asm a.out\n", argv[0]);
        return 1;
    }

    init_ops();

    FILE* src = fopen(argv[1], "r");
    if (!src) {
        fprintf(stderr, "Failed to open source file %s\n", argv[1]);
        return 1;
    }
    FILE* outf = fopen(argv[2], "wb");
    if (!outf) {
        fprintf(stderr, "Failed to open output file %s\n", argv[2]);
        fclose(src);
        return 1;
    }

    char buf[256];
    uint8_t emit[16];
    while (fgets(buf, sizeof(buf), src)) {
        /* skip empty/comment lines (simple ';' comment) */
        char t[256];
        strcpy(t, buf);
        strtrim(t);
        if (t[0] == '\0' || t[0] == ';') continue;

        int len = parse(buf, emit, sizeof(emit));
        if (len > 0) {
            fwrite(emit, 1, (size_t)len, outf);
        } else {
            fprintf(stderr, "Failed to assemble line: %s", buf);
        }
    }

    fclose(src);
    fclose(outf);
    return 0;
}
