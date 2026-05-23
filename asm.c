#define _GNU_SOURCE
#include "gin.h"
#include <ctype.h>
#include <inttypes.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MAX_SYM 4096
#define MAX_LINE 512
#define MAX_TOKS 16
typedef struct {
    char *name;
    uint32_t addr;
} Sym;
static Sym symtab[MAX_SYM];
static int symn = 0;

static uint32_t pc = 0;
static int lineno = 0;
static const char *srcpath = NULL;

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "Error: %s:%d: ", srcpath, lineno);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end >= s && isspace((unsigned char)*end)) *end-- = 0;
    return s;
}

static void add_sym(const char *name, uint32_t addr) {
    for (int i = 0; i < symn; i++) {
        if (strcmp(symtab[i].name, name) == 0)
            die("duplicate symbol '%s'", name);
    }
    if (symn >= MAX_SYM) die("symbol table full");
    symtab[symn].name = strdup(name);
    symtab[symn].addr = addr;
    symn++;
}

static int find_sym(const char *name, uint32_t *out) {
    for (int i = 0; i < symn; i++) {
        if (strcmp(symtab[i].name, name) == 0) {
            *out = symtab[i].addr;
            return 1;
        }
    }
    return 0;
}

enum {
    TYPE_A,
    TYPE_M,
    TYPE_J,
    TYPE_S,
    TYPE_X
};

typedef struct {
    const char *mnem;
    uint32_t code;
    int type;
} OpInfo;

static OpInfo optab[] = {
    {"ADD", 0x00, TYPE_A}, {"SUB", 0x01, TYPE_A}, {"MUL", 0x02, TYPE_A}, {"DIV", 0x03, TYPE_A},
    {"SHL", 0x04, TYPE_A}, {"SHR", 0x05, TYPE_A}, {"AND", 0x06, TYPE_A}, {"OR", 0x07, TYPE_A},
    {"NOT", 0x08, TYPE_A}, {"XOR", 0x09, TYPE_A}, {"LUI", 0x0A, TYPE_A}, {"CMP", 0x0B, TYPE_A},
    {"LDR", 0x0C, TYPE_M}, {"STR", 0x0D, TYPE_M}, {"LDRB",0x0E, TYPE_M}, {"STRB",0x0F, TYPE_M},
    {"JXX", 0x10, TYPE_J}, {"CALL",0x11, TYPE_J}, {"RET",0x12, TYPE_J}, {"PUSH",0x13, TYPE_M},
    {"POP", 0x14, TYPE_M}, {"FLAGS",0x21, TYPE_S}, {"HALT",0x22, TYPE_X}, {"ICALL",0x23, TYPE_J},
    {"IRET",0x24, TYPE_J},
    // condition mnemonics are handled separately (JEQ, JNE, etc.)
    {NULL,0,0}
};

static struct { const char *mnem; int val; } condtab[] = {
    {"JMP", 0x0}, {"JEQ", 0x1}, {"JNE", 0x2}, {"JLT", 0x3}, {"JGE", 0x4},
    {"JLTU",0x5}, {"JGEU",0x6}, {"JCS",0x7}, {"JCC",0x8}, {"JN",0x9},
    {"JP",0xA}, {"JVS",0xB}, {"JVC",0xC}, {"JLS",0xD},
    {NULL, -1}
};

static int lookup_opcode(const char *mnem, uint32_t *opcode, int *type) {
    for (int i = 0; optab[i].mnem; i++) {
        if (strcasecmp(optab[i].mnem, mnem) == 0) {
            *opcode = optab[i].code;
            *type = optab[i].type;
            return 1;
        }
    }
    for (int i = 0; condtab[i].mnem; i++) {
        if (strcasecmp(condtab[i].mnem, mnem) == 0) {
            *opcode = 0x10;
            *type = TYPE_J;
            return 1;
        }
    }
    return 0;
}

static int lookup_cond(const char *mnem, int *cond) {
    for (int i = 0; condtab[i].mnem; ++i) {
        if (strcasecmp(condtab[i].mnem, mnem) == 0) {
            *cond = condtab[i].val;
            return 1;
        }
    }
    return 0;
}

static int is_reg_token(const char *t) {
    if (t[0] != 'R' && t[0] != 'r') return 0;
    char *end;
    long v = strtol(t + 1, &end, 10);
    return (*end == 0 && v >= 0 && v <= 15);
}

static int reg_index(const char *t) {
    return atoi(t + 1);
}

static int parse_number(const char *s, int64_t *out) {
    const char *p = s;
    if (*p != '#') return 0;
    char *end;
    errno = 0;
    long long v = strtoll(p + 1, &end, 0);
    if (errno) return 0;
    if (*end != 0) return 0;
    *out = v;
    return 1;
}

static int eval_expr(const char *expr, int64_t *out) {
    const char *p = expr;
    while (*p && *p != '+' && *p != '-') p++;
    if (*p != '+' && *p != '-') {
        int64_t v;
        if (parse_number(expr, &v)) { *out = v; return 1; }
        uint32_t addr;
        if (find_sym(expr, &addr)) { *out = addr; return 1; }
        return 0;
    }

    char op = *p;
    char left[256], right[256];
    size_t L = p - expr;
    strncpy(left, expr, L); left[L] = 0;
    strcpy(right, p + 1);
    trim(left); trim(right);
    int64_t a, b;
    int a_is_num = parse_number(left, &a);
    int b_is_num = parse_number(right, &b);
    if (!a_is_num) {
        uint32_t addr;
        if (!find_sym(left, &addr)) return 0;
        a = addr;
    }
    if (!b_is_num) {
        uint32_t addr;
        if (!find_sym(right, &addr)) return 0;
        b = addr;
    }
    *out = (op == '+') ? (a +  b) : (a - b);
    return 1;
}

static int tokenise(char *line, char *toks[], int max) {
    int n = 0;
    char *p = line;
    while (*p && n < max) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (*p == ';') break;   // comment
        if (*p == '[' || *p == ']' || *p == '{' || *p == '}' || *p == ',') {
            char tmp[2] = {*p, 0};
            toks[n++] = strdup(tmp);
            p++;
            continue;
        }
        char *start = p;
        while (*p && !isspace((unsigned char)*p) && *p != ',' && *p != '[' && *p != ']' && *p != '{' && *p != '}') p++;
        int len = p - start;
        char *tok = xmalloc(len + 1);
        strncpy(tok, start, len); tok[len] = 0;
        toks[n++] = tok;
    }
    return n;
}

static void free_toks(char *toks[], int n) {
    for (int i = 0; i < n; i++) free(toks[i]);
}

static void write_u32_le(FILE *out, uint32_t v) {
    uint8_t b[4];
    b[0] = v & 0xFF;
    b[1] = (v >> 8) & 0xFF;
    b[2] = (v >> 16) & 0xFF;
    b[3] = (v >> 24) & 0xFF;
    fwrite(b, 1, 4, out);
}

static uint32_t encode_a(uint32_t opcode, int rn, int rd, int is_reg, int is_signed, int64_t immrm) {
    uint32_t w = 0;
    w |= (opcode & 0x3F) << 26;
    w |= (rn & 0xF) << 22;
    w |= (rd & 0xF) << 18;
    if (is_reg) {
        uint32_t rm = (uint32_t)immrm & 0xF;
        w |= (rm & 0xF) << 2;
    } else {
        uint32_t imm = (uint32_t)immrm & 0xFFFF;
        w |= (imm & 0xFFFF) << 2;
    }
    if (is_reg) w |= 1u << 1;
    if (is_signed) w |= 1u << 0;
    return w;
}

static uint32_t encode_m(uint32_t opcode, int rn, int rd, int is_reg, int is_signed, int64_t immrm) {
    return encode_a(opcode, rn, rd, is_reg, is_signed, immrm);
}

static uint32_t encode_j(uint32_t opcode, int cond, int absolute, int is_reg, int signed_flag, int64_t immrm) {
    uint32_t w = 0;
    w |= (opcode & 0x3F) << 26;
    w |= (cond & 0xF) << 22;
    if (absolute) w |= 1u << 21;
    // reserved bits left zero
    if (is_reg) {
        uint32_t rm = (uint32_t)immrm & 0xF;
        w |= (rm & 0xF) << 2;
    } else {
        uint32_t imm16 = (uint32_t)immrm & 0xFFFF;
        w |= (imm16 & 0xFFFF) << 2;
    }
    if (is_reg) w |= 1u << 1;
    if (signed_flag) w |= 1u << 0;
    return w;
}

static uint32_t encode_s(uint32_t opcode, int rd, int is_reg, int rw, int64_t immrm) {
    uint32_t w = 0;
    w |= (opcode & 0x3F) << 26;
    w |= (rd & 0xF) << 22;
    if (is_reg) {
        uint32_t rm = (uint32_t)immrm & 0xF;
        w |= (rm & 0xF) << 6;
    } else {
        uint32_t imm16 = (uint32_t)immrm & 0xFFFF;
        w |= (imm16 & 0xFFFF) << 6;
    }
    if (is_reg) w |= 1u << 1;
    if (rw) w |= 1u << 0;
    return w;
}

static void pass1(FILE *f) {
    char linebuf[MAX_LINE];
    pc = 0;
    lineno = 0;
    while (fgets(linebuf, sizeof(linebuf), f)) {
        lineno++;
        char *line = trim(linebuf);
        if (*line == 0) continue;
        char *c = strchr(line, ';');
        if (c) *c = 0;
        char *colon = strchr(line, ':');
        if (colon) {
            char lab[256];
            int L = colon - line;
            strncpy(lab, line, L); lab[L] = 0;
            char *labtrim = trim(lab);
            if (strlen(labtrim) == 0) die("empty label");
            add_sym(labtrim, pc);
            char *rest = trim(colon + 1);
            if (*rest == 0) continue;
            line = rest;
        }
        if (line[0] == '.') {
            if (strncmp(line, ".org", 4) == 0) {
                char *arg = trim(line + 4);
                int64_t v;
                if (!eval_expr(arg, &v)) die("unknown symbol in .org");
                if (v % 4 != 0) die(".org must be word aligned");
                pc = (uint32_t)v;
                continue;
            }
            else if (strncmp(line, ".byte", 5) == 0) { pc += 1; continue; }
            else if (strncmp(line, ".word", 5) == 0) { pc += 4; continue; }
            else if (strncmp(line, ".align", 6) == 0) {
                char *arg = trim(line + 6);
                int64_t v;
                if (!eval_expr(arg, &v)) die("unknown symbol in .align");
                uint32_t to_align = (uint32_t)v - (pc % (uint32_t)v);
                pc += to_align;
                continue;
            }
            else {
                die("unknown directive '%s'", line);
            }
        }
        pc += 4;
    }
}

static void pass2(FILE *f, FILE *out) {
    char linebuf[MAX_LINE];
    pc = 0;
    lineno = 0;
    while (fgets(linebuf, sizeof(linebuf), f)) {
        lineno++;
        char *raw = strdup(linebuf);
        char *line = trim(raw);
        if (*line == 0) { free(raw); continue; }
        char *c = strchr(line, ';'); if (c) *c = 0;
        char *colon = strchr(line, ':');
        if (colon) {
            char *rest = trim(colon + 1);
            if (*rest == 0) { free(raw); continue; }
            line = rest;
        }
        if (line[0] == '.') {
            if (strncmp(line, ".org", 4) == 0) {
                char *arg = trim(line + 4);
                int64_t v;
                if (!eval_expr(arg, &v)) die("unknown symbol in .org");
                if (v % 4 != 0) die(".org must be word aligned");
                pc = (uint32_t)v;
                continue;
            }
            else if (strncmp(line, ".byte", 5) == 0) {
                char *arg = trim(line + 5);
                int64_t v;
                if (!eval_expr(arg, &v)) die("unknown symbol in .byte");
                fwrite(&v, 1, 1, out);
                pc += 1;
                continue;
            }
            else if (strncmp(line, ".word", 5) == 0) {
                char *arg = trim(line + 5);
                int64_t v;
                if (!eval_expr(arg, &v)) die("unknown symbol in .word");
                write_u32_le(out, (uint32_t)v);
                pc += 4;
                continue;
            }
            else if (strncmp(line, ".align", 6) == 0) {
                char *arg = trim(line + 6);
                int64_t v;
                if (!eval_expr(arg, &v)) die("unknown symbol in .align");
                uint32_t to_align = (uint32_t)v - (pc % (uint32_t)v);
                for (uint32_t i = 0; i < to_align; i++) {
                    fwrite("\0", 1, 1, out);
                }
                pc += to_align;
                continue;
            }
            else {
                die("unknown directive '%s'", line);
            }
        }

        char *toks[MAX_TOKS];
        int tn = tokenise(line, toks, MAX_TOKS);
        if (tn == 0) { free(raw); continue; }
        char *mnem = toks[0];
        if (strcasecmp(mnem, "HALT") == 0) {
            uint32_t word = (0x22u << 26);
            write_u32_le(out, word);
            pc += 4; free_toks(toks, tn); free(raw); continue;
        }
        if (strcasecmp(mnem, "RET") == 0) {
            uint32_t word = (0x12u << 26);
            write_u32_le(out, word);
            pc += 4; free_toks(toks, tn); free(raw); continue;
        }
        uint32_t opcode; int type;
        if (!lookup_opcode(mnem, &opcode, &type)) {
            die("unknown mnemonic '%s'", mnem);
        }
        if (type == TYPE_J) {
            int cond = 0;
            if (!lookup_cond(mnem, &cond)) {
                if (strcasecmp(mnem, "CALL") == 0) cond = 0;
                else if (strcasecmp(mnem, "ICALL") == 0) cond = 0;
                else cond = 0;
            }
            int absolute = 0;
            int is_reg = 0;
            int signed_flag = 1;
            int64_t imm = 0;
            if (tn < 2) die("missing operand for %s", mnem);
            int idx = 1;
            if (tn >= 3 && strcasecmp(toks[1], "abs") == 0) { absolute = 1; idx = 2; }
            char *op = toks[idx];
            if (is_reg_token(op)) {
                is_reg = 1;
                imm = reg_index(op);
            } else {
                if (!eval_expr(op, &imm)) die("unknown symbol in operand '%s'", op);
                if (!absolute) {
                    int64_t offset = imm - (int64_t)(pc + 4);
                    imm = offset;
                }
                if (!is_reg && (imm < -32768 || imm > 32767)) {
                    die("jump offset out of range: %" PRId64, imm);
                }
            }
            uint32_t word = encode_j(opcode, cond, absolute, is_reg, signed_flag, imm);
            write_u32_le(out, word);
            pc += 4; free_toks(toks, tn); free(raw); continue;
        }
        if (type == TYPE_A || type == TYPE_M) {
            if (strcasecmp(mnem, "PUSH") == 0 || strcasecmp(mnem, "POP") == 0) {
                if (tn < 2) die("missing operand for %s", mnem);
                char *op = toks[1];
                int is_reg = 0;
                int64_t imm = 0;
                if (is_reg_token(op)) {
                    is_reg = 1; imm = reg_index(op);
                } else {
                    char tmp[256];
                    strcpy(tmp, op);
                    if (tmp[0] == '{' && tmp[strlen(tmp) - 1] == '}') {
                        tmp[strlen(tmp) - 1] = 0;
                        memmove(tmp, tmp + 1, strlen(tmp));
                    }
                    if (!eval_expr(tmp, &imm)) die("invalid push/pop mask '%s'", op);
                    is_reg = 0;
                }
                uint32_t opcode_val = (strcasecmp(mnem, "PUSH") == 0) ? 0x13 : 0x14;
                uint32_t word = encode_m(opcode_val, 0, 0, is_reg, 1, imm);
                write_u32_le(out, word);
                pc += 4; free_toks(toks, tn); free(raw); continue;
            }

            char *ops[MAX_TOKS];
            int opn = 0;
            for (int i = 1; i < tn && opn < MAX_TOKS; i++) {
                if (strcmp(toks[i], ",") == 0) continue;
                ops[opn++] = toks[i];
            }

            if (strcasecmp(mnem, "LUI") == 0 || strcasecmp(mnem, "NOT") == 0 || strcasecmp(mnem, "CMP") == 0) {
                if (opn != 2) die("expected '%s RD, OP'", mnem);
                char *first = ops[0];
                char *optok = ops[1];
                if (!optok) die("missing operand");
                int is_reg = 0;
                int64_t imm = 0;
                if (is_reg_token(optok)) { is_reg = 1; imm = reg_index(optok); }
                else {
                    if (!eval_expr(optok, &imm)) die("unknown symbol in operand '%s'", optok);
                }
                int signed_flag = 1;
                uint32_t word = 0;
                if (strcasecmp(mnem, "NOT") == 0) {
                    if (!is_reg_token(first)) die("invalid RD '%s'", first);
                    int rd = reg_index(first);
                    word = encode_a(0x08, 0, rd, is_reg, signed_flag, imm);
                } else if (strcasecmp(mnem, "LUI") == 0) {
                    if (!is_reg_token(first)) die("invalid RD '%s'", first);
                    int rd = reg_index(first);
                    word = encode_a(0x0A, 0, rd, is_reg, signed_flag, imm);
                } else if (strcasecmp(mnem, "CMP") == 0) {
                    if (!is_reg_token(first)) die("invalid RN '%s'", first);
                    int rn = reg_index(first);
                    word = encode_a(0x0B, rn, 0, is_reg, signed_flag, imm);
                }
                write_u32_le(out, word);
                pc += 4; free_toks(toks, tn); free(raw); continue;
            }

            if (type == TYPE_M && opn >= 3 && strcmp(ops[1], "[") == 0) {
                char *rdtok = ops[0];
                if (!is_reg_token(rdtok)) die("invalid RD '%s'", rdtok);
                int rd = reg_index(rdtok);

                int close_bracket = -1;
                for (int i = 1; i < opn; i++) {
                    if (strcmp(ops[i], "]") == 0) {
                        close_bracket = i;
                        break;
                    }
                }
                if (close_bracket == -1) die("missing ] in memory addressing");

                int rn = 0;
                int64_t imm = 0;
                int is_reg = 0;
                int idx = 2;

                if (idx < close_bracket && is_reg_token(ops[idx])) {
                    rn = reg_index(ops[idx]);
                    idx++;
                }

                if (idx < close_bracket && (strcmp(ops[idx], "+") == 0 || strcmp(ops[idx], "-") == 0)) {
                    int is_add = (strcmp(ops[idx], "+") == 0);
                    idx++;

                    if (idx >= close_bracket) die("expected offset after + or -");

                    if (is_reg_token(ops[idx])) {
                        is_reg = 1;
                        imm = reg_index(ops[idx]);
                    } else {
                        if (!eval_expr(ops[idx], &imm)) die("invalid offset '%s'", ops[idx]);
                    }

                    if (!is_add) imm = -imm;
                } else if (idx == 2 && idx < close_bracket) {
                    if (is_reg_token(ops[idx])) {
                        is_reg = 1;
                        imm = reg_index(ops[idx]);
                    } else {
                        if (!eval_expr(ops[idx], &imm)) die("invalid offset '%s'", ops[idx]);
                    }
                }

                uint32_t opc = 0;
                if (!lookup_opcode(mnem, &opc, &type)) die("unknown M-type '%s'", mnem);
                int signed_flag = 1;
                uint32_t word = encode_m(opc, rn, rd, is_reg, signed_flag, imm);
                write_u32_le(out, word);
                pc += 4; free_toks(toks, tn); free(raw); continue;
            }

            if (opn != 3) die("expected 'MNEM RD, RN, OP'");
            char *rdtok = ops[0];
            char *rntok = ops[1];
            char *optok = ops[2];
            if (!is_reg_token(rdtok)) die("invalid RD '%s'", rdtok);
            if (!is_reg_token(rntok)) die("invalid RN '%s'", rntok);
            int rd = reg_index(rdtok);
            int rn = reg_index(rntok);
            int is_reg = 0;
            int64_t imm = 0;
            if (is_reg_token(optok)) { is_reg = 1; imm = reg_index(optok); }
            else {
                if (!eval_expr(optok, &imm)) die("unknown symbol in operand '%s'", optok);
            }
            int signed_flag = 1;
            uint32_t word = 0;
            if (type == TYPE_A) {
                uint32_t opc = 0;
                if (!lookup_opcode(mnem, &opc, &type)) die("unknown A-type '%s'", mnem);
                word = encode_a(opc, rn, rd, is_reg, signed_flag, imm);
            } else {
                uint32_t opc = 0;
                if (!lookup_opcode(mnem, &opc, &type)) die("unknown M-type '%s'", mnem);
                word = encode_m(opc, rn, rd, is_reg, signed_flag, imm);
            }
            write_u32_le(out, word);
            pc += 4; free_toks(toks, tn); free(raw); continue;
        }
        if (type == TYPE_S) {
            if (tn < 2) die("FLAGS requires at least RD");
            char *rdtok = toks[1];
            if (!is_reg_token(rdtok)) die("invalid RD '%s'", rdtok);
            int rd = reg_index(rdtok);
            if (tn == 2) {
                uint32_t word = encode_s(0x21, rd, 0, 0, 0);
                write_u32_le(out, word);
                pc += 4; free_toks(toks, tn); free(raw); continue;
            } else {
                char *sep = toks[2];
                char *op = toks[3];
                if (strcmp(sep, ",") != 0) {
                    size_t L = strlen(rdtok);
                    if (rdtok[L - 1] == ',') {
                        rdtok[L - 1] = 0;
                        op = toks[2];
                    } else {
                        die("expected comma after RD");
                    }
                }
                int is_reg = 0;
                int64_t imm = 0;
                if (is_reg_token(op)) { is_reg = 1; imm = reg_index(op); }
                else {
                    if (!eval_expr(op, &imm)) die("invalid FLAGS operand '%s'", op);
                }
                uint32_t word = encode_s(0x21, rd, is_reg, 1, imm);
                write_u32_le(out, word);
                pc += 4; free_toks(toks, tn); free(raw); continue;
            }
        }

        die("unhandled instruction '%s'", mnem);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s input.s\n", argv[0]);
        return 1;
    }
    srcpath = argv[1];
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); return 1; }
    pass1(f);
    rewind(f);
    FILE *out = fopen("out.bin", "wb");
    if (!out) { perror("fopen out.bin"); return 1; }
    pass2(f, out);
    fclose(f);
    fclose(out);
    printf("Assembled %s -> out.bin (symbols: %d)\n", argv[1], symn);
    return 0;
}