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
#include "isa.h"

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
    if (strncmp(t + 1, "sp", 2) == 0) return 1;
    long v = strtol(t + 1, &end, 10);
    return (*end == 0 && v >= 0 && v <= 15);
}

static int reg_index(const char *t) {
    if (strncmp(t + 1, "sp", 2) == 0) return 15;
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

static int eval_expr(const char *expr, int64_t *out);

static int parse_dollar(const char *s, int64_t *out) {
    if(!s || s[0] != '$') return 0;
    const char *p = s + 1;
    const char *q = p;
    while (*q && *q != '+' && *q != '-') q++;
    if (*q == '+' || *q == '-') {
        char left[256], right[256];
        size_t L = q - p;
        if (L >= sizeof(left)) return 0;
        strncpy(left, p, L); left[L] = 0;
        strcpy(right, q + 1);
        trim(left); trim(right);
        uint32_t addr;
        if (!find_sym(left, &addr)) return 0;
        int64_t b;
        if (!parse_number(right, &b) && !eval_expr(right, &b)) return 0;
        *out = (q[0] == '+') ? (int64_t)addr + b : (int64_t)addr - b;
        return 1;
    } else {
        char name[256];
        strncpy(name, p, sizeof(name) - 1); name[sizeof(name) - 1] = 0;
        trim(name);
        uint32_t addr;
        if (!find_sym(name, &addr)) return 0;
        *out = (int64_t)addr;
        return 1;
    }
}

static int eval_expr(const char *expr, int64_t *out) {
    if (!expr) return 0;
    char tmp[256];
    strncpy(tmp, expr, sizeof(tmp)-1); tmp[sizeof(tmp)-1]=0;
    char *t = trim(tmp);

    if (t[0] == '$') {
        return parse_dollar(t, out);
    }

    const char *p = t;
    while (*p && *p != '+' && *p != '-') p++;
    if (*p != '+' && *p != '-') {
        int64_t v;
        if (parse_number(t, &v)) { *out = v; return 1; }
        uint32_t addr;
        if (find_sym(t, &addr)) { *out = addr; return 1; }
        return 0;
    }

    char op = *p;
    char left[256], right[256];
    size_t L = p - t;
    if (L >= sizeof(left)) return 0;
    strncpy(left, t, L); left[L]=0;
    strcpy(right, p+1);
    trim(left); trim(right);

    int64_t a, b;
    if (left[0] == '$') {
        if (!parse_dollar(left, &a)) return 0;
    } else if (!parse_number(left, &a)) {
        uint32_t addr;
        if (!find_sym(left, &addr)) return 0;
        a = addr;
    }

    if (right[0] == '$') {
        if (!parse_dollar(right, &b)) return 0;
    } else if (!parse_number(right, &b)) {
        uint32_t addr;
        if (!find_sym(right, &addr)) return 0;
        b = addr;
    }

    *out = (op == '+') ? (a + b) : (a - b);
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
        char *tt = trim(tok);
        toks[n++] = strdup(tt);
        free(tok);
    }
    return n;
}

static void free_toks(char *toks[], int n) {
    for (int i = 0; i < n; i++) free(toks[i]);
}

// ensure the output file has been padded with zeros up to 'pc' before writing
static void ensure_out_pos(FILE *out, uint32_t pc) {
    long cur = ftell(out);
    if (cur < 0) die("ftell failed");
    if ((uint32_t)cur > pc) {
        // writing backwards would overwrite; this is an error for flat binary mode
        die("assembler attempted to write at 0x%08x but file pos is 0x%08lx",
            srcpath, lineno, pc, cur);
    }
    // pad with zeros
    while ((uint32_t)cur < pc) {
        uint8_t z = 0;
        if (fwrite(&z, 1, 1, out) != 1) die("failed to write padding");
        cur++;
    }
}

static void write_u32_le(FILE *out, uint32_t v) {
    ensure_out_pos(out, pc);
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
            if (strncmp(line, ".byte", 5) == 0) {
                char *arg = trim(line + 5);
                // count comma-separated entries
                char *p = arg;
                while (*p) {
                    char tok[256]; int i = 0;
                    while (*p && *p != ',') tok[i++] = *p++;
                    tok[i] = 0;
                    if (*p == ',') p++;
                    char *tt = trim(tok);
                    if (tt[0] == 0) die("empty .byte element");
                    int64_t v;
                    if (!eval_expr(tt, &v)) die("unknown symbol in .byte '%s'", tt);
                    pc += 1;
                }
                continue;
            }
            else if (strncmp(line, ".word", 5) == 0) {
                char *arg = trim(line + 5);
                char *p = arg;
                while (*p) {
                    char tok[256]; int i = 0;
                    while (*p && *p != ',') tok[i++] = *p++;
                    tok[i] = 0;
                    if (*p == ',') p++;
                    char *tt = trim(tok);
                    if (tt[0] == 0) die("empty .word element");
                    int64_t v;
                    if (!eval_expr(tt, &v)) die("unknown symbol in .word '%s'", tt);
                    // optional: require word alignment in pass1
                    if (pc % 4 != 0) die(".word requires word-aligned address (pc=0x%08x)", pc);
                    pc += 4;
                }
                continue;
            }
            else if (strncmp(line, ".align", 6) == 0) {
                char *arg = trim(line + 6);
                int64_t n;
                if (!eval_expr(arg, &n)) die("invalid .align argument '%s'", arg);
                if (n <= 0) die(".align requires positive power-of-two");
                // check power of two
                if ((n & (n - 1)) != 0) die(".align argument must be power of two");
                // compute new pc
                uint32_t mask = (uint32_t)(n - 1);
                pc = (pc + mask) & ~mask;
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
            else if (strncmp(line, ".align", 6) == 0) {
                char *arg = trim(line + 6);
                int64_t n;
                if (!eval_expr(arg, &n)) die("invalid .align argument '%s'", arg);
                if (n <= 0 || (n & (n - 1)) != 0) die(".align requires positive power-of-two");
                uint32_t mask = (uint32_t)(n - 1);
                pc = (pc + mask) & ~mask;
                free(raw);
                continue;
            }
            else if (strncmp(line, ".byte", 5) == 0) {
                char *arg = trim(line + 5);
                char *p = arg;
                while (*p) {
                    char tok[256]; int i = 0;
                    while (*p && *p != ',') tok[i++] = *p++;
                    tok[i] = 0;
                    if (*p == ',') p++;
                    char *tt = trim(tok);
                    int64_t v;
                    if (!eval_expr(tt, &v)) die("invalid .byte '%s'", tt);
                    if (v < 0 || v > 0xFF) die(".byte value out of range 0..0xFF: %" PRId64, v);
                    uint8_t b = (uint8_t)v & 0xFF;
                    ensure_out_pos(out, pc);
                    fwrite(&b, 1, 1, out);
                    pc += 1;
                }
                free(raw);
                continue;
            }
            else if (strncmp(line, ".word", 5) == 0) {
                char *arg = trim(line + 5);
                char *p = arg;
                while (*p) {
                    char tok[256]; int i = 0;
                    while (*p && *p != ',') tok[i++] = *p++;
                    tok[i] = 0;
                    if (*p == ',') p++;
                    char *tt = trim(tok);
                    int64_t v;
                    if (!eval_expr(tt, &v)) die("invalid .word '%s'", tt);
                    if (pc % 4 != 0) die(".word requires word-aligned address (pc=0x%08x)", pc);
                    write_u32_le(out, (uint32_t)v);
                    pc += 4;
                }
                free(raw);
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
        if (strcasecmp(mnem, "MOV") == 0) { // superpowerful pseudoinstruction
            char *ops[MAX_TOKS]; int opn = 0;
            int i = 1;
            while (i < tn && opn < MAX_TOKS) {
                if (strcmp(toks[i], ",") == 0) { i++; continue; }

                if (strcmp(toks[i], "[") == 0 || toks[i][0] == '[') {
                    char buf[256]; size_t pos = 0;
                    int depth = 0;
                    while (i < tn && pos + 1 < sizeof(buf)) {
                        char tmp[128];
                        strncpy(tmp, toks[i], sizeof(tmp)-1); tmp[sizeof(tmp)-1]=0;
                        char *t = tmp;
                        for (char *p = t; *p; ++p) if (*p == '[') depth++;
                        for (char *p = t; *p; ++p) if (*p == ']') depth--;
                        if (pos) buf[pos++] = ' ';
                        size_t L = strlen(t);
                        if (pos + L >= sizeof(buf)) die("%s:%d: operand too long", srcpath, lineno);
                        memcpy(buf + pos, t, L); pos += L;
                        i++;
                        if (depth <= 0) break;
                    }
                    buf[pos] = 0;
                    char *s = trim(buf);
                    ops[opn++] = strdup(s);
                    continue;
                }

                if (strcmp(toks[i], "{") == 0 || toks[i][0] == '{') {
                    char buf[256]; size_t pos = 0;
                    int depth = 0;
                    while (i < tn && pos + 1 < sizeof(buf)) {
                        char tmp[128];
                        strncpy(tmp, toks[i], sizeof(tmp)-1); tmp[sizeof(tmp)-1]=0;
                        char *t = tmp;
                        for (char *p = t; *p; ++p) if (*p == '{') depth++;
                        for (char *p = t; *p; ++p) if (*p == '}') depth--;
                        if (pos) buf[pos++] = ' ';
                        size_t L = strlen(t);
                        if (pos + L >= sizeof(buf)) die("%s:%d: operand too long", srcpath, lineno);
                        memcpy(buf + pos, t, L); pos += L;
                        i++;
                        if (depth <= 0) break;
                    }
                    buf[pos] = 0;
                    char *s = trim(buf);
                    ops[opn++] = strdup(s);
                    continue;
                }

                ops[opn++] = strdup(trim(toks[i]));
                i++;
            }

            char *dst = trim(ops[0]);
            char *src = trim(ops[1]);

            if (dst[0] == '{' || strcmp(ops[0], "{") == 0) {
                if (!is_reg_token(src)) die("%s:%d: MOV source must be a register for MOV {..}, Rs", srcpath, lineno);

                int rs = reg_index(src);

                if (strcmp(ops[0], "{") == 0) {
                    int idx = 1;
                    int emitted = 0;
                    if (idx >= opn) die("%s:%d: empty register list in MOV", srcpath, lineno);
                    while (idx < opn) {
                        char *regtok = ops[idx++];
                        if (strcmp(regtok, "}") == 0) break;
                        if (!is_reg_token(regtok)) die("%s:%d: expected register in list, got '%s'", srcpath, lineno, regtok);
                        int rd = reg_index(regtok);
                        if (idx >= opn) die("%s:%d: malformed register list, missing '}'", srcpath, lineno);
                        char *sep = ops[idx++];
                        if (strcmp(sep, ",") == 0) {
                            write_u32_le(out, encode_a(OP_XOR, rd, rd, 1, 0, rd));
                            uint32_t w = encode_a(OP_ADD, rs, rd, 0, 1, 0);
                            write_u32_le(out, w); pc += 4;
                            emitted++;
                            continue;
                        } else if (strcmp(sep, "}") == 0) {
                            write_u32_le(out, encode_a(OP_XOR, rd, rd, 1, 0, rd));
                            uint32_t w = encode_a(OP_ADD, rs, rd, 0, 1, 0);
                            write_u32_le(out, w); pc += 4;
                            emitted++;
                            break;
                        } else {
                            die("%s:%d: expected ',' between registers in list, got '%s'", srcpath, lineno, sep);
                        }
                    }
                    if (emitted == 0) die("%s:%d: empty register list in MOV", srcpath, lineno);
                    free_toks(toks, tn);
                    for (int k = 0; k < opn; ++k) free(ops[k]);
                    free(raw);
                    continue;
                }

                // If we reach here, the brace form wasn't recognized
                for (int k = 0; k < opn; ++k) free(ops[k]);
                // fall through to other MOV handling
            }

            if (is_reg_token(dst) && is_reg_token(src)) {
                int rd = reg_index(dst);
                int rs = reg_index(src);
                uint32_t word = encode_a(OP_ADD, rs, rd, 0, 1, 0);
                write_u32_le(out, word);
                pc += 4; free_toks(toks, tn); free(raw); continue;
            }

            if (is_reg_token(dst) && src[0] == '{') {
                char inner[256];
                size_t L = strlen(src);
                if (L < 2 || src[L-1] != '}') die("%s:%d: malformed register list '%s'", srcpath, lineno, src);
                strncpy(inner, src + 1, L - 2);
                inner[L - 2] = 0;
                char *p = trim(inner);
                if (p[0] == 0) die("%s:%d: empty register list in MOV", srcpath, lineno);

                uint32_t mask = 0;
                char *tok = strtok(p, ",");
                while (tok) {
                    char *r = trim(tok);
                    if (!is_reg_token(r)) die("%s:%d: expected register in list, got '%s'", srcpath, lineno, r);
                    int ri = reg_index(r);
                    if (ri < 0 || ri > 15) die("%s:%d: register out of range in list '%s'", srcpath, lineno, r);
                    mask |= (1u << ri);
                    tok = strtok(NULL, ",");
                }

                int rd = reg_index(dst);
                uint32_t w_xor = encode_a(OP_XOR, rd, rd, 1, 0, rd);
                write_u32_le(out, w_xor);
                pc += 4;
                if (mask != 0) {
                    uint32_t w_add = encode_a(OP_ADD, rd, rd, 0, 1, (int64_t)mask);
                    write_u32_le(out, w_add);
                    pc += 4;
                }

                free_toks(toks, tn);
                free(raw);
                continue;
            }

            if (is_reg_token(dst) && !is_reg_token(src) && (src[0] == '#' || src[0] == '$')) {
                int rd = reg_index(dst);
                int64_t val;
                if (!parse_number(src, &val) && !parse_dollar(src, &val) && !eval_expr(src, &val)) die("invalid immediate '%s'", src);
                uint32_t high = (uint32_t)((uint64_t)(val) >> 16) & 0xFFFFu;
                uint32_t low = (uint32_t)(val & 0xFFFFu);
                if (high != 0) {
                    uint32_t w_lui = encode_a(OP_LUI, 0, rd, 0, 1, (int64_t)high);
                    write_u32_le(out, w_lui);
                    pc += 4;
                } else {
                    uint32_t w_xor = encode_a(OP_XOR, rd, rd, 1, 0, rd);
                    write_u32_le(out, w_xor);
                    pc += 4;
                }
                if (low != 0) {
                    uint32_t w_add = encode_a(OP_ADD, rd, rd, 0, 1, (int64_t)low);
                    write_u32_le(out, w_add);
                    pc += 4;
                }
                free_toks(toks, tn); free(raw); continue;
            }

            if (is_reg_token(dst) && src[0] == '[') {
                int bidx = -1;
                for (int i = 0; i < tn; i++) if (toks[i][0] == '[') { bidx = i; break; }
                if (bidx == -1) die("malformed memory operand");
                int close = -1;
                for (int i = bidx + 1; i < tn; i++) if (toks[i][0] == ']') { close = i; break; }
                if (close == -1) die("missing ']'");
                if (bidx + 1 >= close) die("empty memory operand");

                char *base_tok = trim(toks[bidx + 1]);
                if (base_tok[0] == '[') base_tok = trim(base_tok + 1);
                size_t Lb = strlen(base_tok);
                if (Lb > 0 && base_tok[Lb - 1] == ']') { base_tok[Lb - 1] = 0; base_tok = trim(base_tok); }

                if (!is_reg_token(base_tok)) die("expected base register in memory operand, got '%s'", base_tok);
                int rn = reg_index(base_tok);
                int is_reg_off = 0;
                int64_t off = 0;

                if (bidx + 2 < close) {
                    char *opsep = trim(toks[bidx + 2]);
                    if (!(opsep[0] == '+' || opsep[0] == '-')) die("expected '+' or '-' in memory operand, got '%s'", opsep);
                    int sign = (opsep[0] == '+') ? 1 : -1;
                    if (bidx + 3 >= close) die("missing offset after '%s'", opsep);
                    char *offtok = trim(toks[bidx + 3]);
                    size_t Lo = strlen(offtok);
                    if (Lo > 0 && offtok[Lo - 1] == ']') { offtok[Lo - 1] = 0; offtok = trim(offtok); }
                    if (is_reg_token(offtok)) { is_reg_off = 1; off = reg_index(offtok); }
                    else if (!parse_number(offtok, &off) && !eval_expr(offtok, &off)) die("invalid offset '%s'", offtok);
                    off = sign * off;
                }

                int rd = reg_index(dst);
                uint32_t word = encode_m(OP_LDR, rn, rd, is_reg_off, 1, off);
                write_u32_le(out, word);
                pc += 4; free_toks(toks, tn); free(raw); continue;
            }


            if (is_reg_token(src) && dst[0] == '[') {
                int bidx = -1;
                for (int i = 1; i < tn; ++i) if (strcmp(toks[i], "[") == 0) { bidx = i; break; }
                if (bidx == -1) die("malformed memory operand");
                int close = -1;
                for (int i = bidx+1; i < tn; ++i) if (strcmp(toks[i], "]") == 0) { close = i; break; }
                if (close == -1) die("missing ']'");
                if (bidx + 1 >= close) die("empty memory operand");
                char *base_tok = trim(toks[bidx+1]);
                if (!is_reg_token(base_tok)) die("expected base register in memory operand, got '%s'", base_tok);
                int rn = reg_index(base_tok);
                int is_reg_off = 0;
                int64_t off = 0;
                if (bidx + 2 < close) {
                    char *opsep = trim(toks[bidx+2]);
                    if (strcmp(opsep, "+") != 0 && strcmp(opsep, "-") != 0) die("expected '+' or '-' in memory operand, got '%s'", opsep);
                    int sign = (opsep[0] == '+') ? 1 : -1;
                    if (bidx + 3 >= close) die("missing offset after '%s'", opsep);
                    char *offtok = trim(toks[bidx+3]);
                    if (is_reg_token(offtok)) { is_reg_off = 1; off = reg_index(offtok); }
                    else {
                        if (!parse_number(offtok, &off) && !eval_expr(offtok, &off)) die("invalid offset '%s'", offtok);
                    }
                    off = sign * off;
                }
                int rs = reg_index(src);
                uint32_t word = encode_m(OP_STR, rn, rs, is_reg_off, 1, off);
                write_u32_le(out, word);
                pc += 4;
                free_toks(toks, tn); free(raw); continue;
            }

            die("unknown MOV form");
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
                    if (op[0] != '{') die("malformed mask '%s'", op);
                    op = toks[2];
                    if (is_reg_token(op)) {
                        int idx =2;
                        if (idx >= tn) die("empty mask");
                        int found_close = 0;
                        uint32_t mask = 0;
                        int saw_any = 0;

                        if (idx < tn && toks[idx][0] == '}') die("empty mask");
                        if (idx < tn) {
                            char *item = trim(toks[idx++]);
                            if (!is_reg_token(item)) die("expected register in mask");
                            int r = reg_index(item);
                            if (r < 0 || r > 15) die("register out of range in mask");
                            mask |= (1u << r);
                            saw_any = 1;
                        } else die("malformed mask");

                        while (idx < tn) {
                            char *sep = toks[idx++];
                            if (sep[0] == '}') { found_close = 1; break; }
                            if (sep[0] != ',') die("expected ',' in between mask registers");
                            char *item = trim(toks[idx++]);
                            if (item[0] == 0) continue;
                            if (!is_reg_token(item)) die("Expected register in mask, got '%s'", item);
                            int r = reg_index(item);
                            if (r < 0 || r > 15) die("register out of range in mask '%s'", item);
                            mask |= (1u << r);
                            saw_any =1;
                        }

                        if (!found_close) die("malformed mask, missing '}'");
                        if (!saw_any) die("empty register list in mask");

                        imm = (int64_t)mask;
                        is_reg = 0;
                    } else {
                        if (!parse_number(op, &imm) && !eval_expr(op, &imm)) die("invalid push/pop mask '%s'", op);
                        if (imm > (1u << 15)) die("can only push/pop r0-15");
                    }
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
                    word = encode_a(OP_NOT, 0, rd, is_reg, signed_flag, imm);
                } else if (strcasecmp(mnem, "LUI") == 0) {
                    if (!is_reg_token(first)) die("invalid RD '%s'", first);
                    int rd = reg_index(first);
                    word = encode_a(OP_LUI, 0, rd, is_reg, signed_flag, imm);
                } else if (strcasecmp(mnem, "CMP") == 0) {
                    if (!is_reg_token(first)) die("invalid RN '%s'", first);
                    int rn = reg_index(first);
                    word = encode_a(OP_CMP, rn, 0, is_reg, signed_flag, imm);
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
                uint32_t word = encode_s(OP_FLAGS, rd, 0, 0, 0);
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
                uint32_t word = encode_s(OP_FLAGS, rd, is_reg, 1, imm);
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