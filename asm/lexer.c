#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int is_mnemonic(const char *str) {
    const char *mnemonics[] = {
        "mov", 
        "push",
        "pop",
        "nop",
        "sub",
        "add",
        "mul",
        "div",
        "divu",
        "shl",
        "shr",
        "shru",
        "and",
        "or",
        "xor",
        "lui",
        "ldr",
        "str",
        "ldrb",
        "strb",
        "jmp",
        "jmpa",
        "jeq",
        "jne",
        "jlt",
        "jge",
        "jltu",
        "jgeu",
        "call",
        "calla",
        "ret",
        "fadd",
        "fsub",
        "fmul",
        "fdiv",
        "fcmp",
        "itof",
        "ftoi",
        "xchg",
        "fence",
        "rpc",
        "halt",
        "syscall",
        "iret",
        NULL
    };
    for (int i = 0; mnemonics[i] != NULL; i++) {
        if (strcasecmp(str, mnemonics[i]) == 0) return 1;
    }
    return 0;
}

static int parse_register(const char *str, int64_t *out_reg) {
    if (strcasecmp(str, "rsp") == 0 || strcasecmp(str, "sp") == 0) {
        *out_reg = 15;
        return 1;
    }
    if ((str[0] == 'r' || str[0] == 'R') && isdigit(str[1])) {
        char *end;
        long regnum = strtol(str + 1, &end, 10);
        if (*end == '\0' && regnum >= 0 && regnum <= 15) {
            *out_reg = regnum;
            return 1;
        }
    }
    return 0;
}

static int is_directive_keyword(const char *str) {
    const char *directives[] = {
        ".section", ".byte", ".word", NULL
    };
    for (int i = 0; directives[i] != NULL; i++) {
        if (strcasecmp(str, directives[i]) == 0) return 1;
    }
    return 0;
}

token_array lex(char *source) {
    token_array tags = token_array_init();

    char *p = source;
    int line = 1;

    while (*p != '\0') {
        if (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
            continue;
        }
        if (*p == '\n') {
            line++;
            p++;
            continue;
        }

        if (*p == ';') {
            while (*p != '\0' && *p != '\n') {
                p++;
            }
            continue;
        }

        Token tok;
        memset(&tok, 0, sizeof(Token));
        tok.line_num = line;

        if (*p == ',') { tok.type = TOK_COMMA;  p++; token_array_push(&tags, tok); continue; }
        if (*p == '[') { tok.type = TOK_LBRACK; p++; token_array_push(&tags, tok); continue; }
        if (*p == ']') { tok.type = TOK_RBRACK; p++; token_array_push(&tags, tok); continue; }
        if (*p == '{') { tok.type = TOK_LBRACE; p++; token_array_push(&tags, tok); continue; }
        if (*p == '}') { tok.type = TOK_RBRACE; p++; token_array_push(&tags, tok); continue; }
        if (*p == '+') { tok.type = TOK_PLUS;   p++; token_array_push(&tags, tok); continue; }
        if (*p == '-') { tok.type = TOK_MINUS;  p++; token_array_push(&tags, tok); continue; }

        if (*p == '.') {
            int i = 0;
            tok.string[i++] = *p++;
            
            while (*p != '\0' && (isalnum(*p) || *p == '_')) {
                if (i < 31) tok.string[i++] = *p;
                p++;
            }
            tok.string[i] = '\0';

            if (*p == ':') {
                tok.type = TOK_LABEL_DEF;
                p++;
            } 
            else if (is_directive_keyword(tok.string)) {
                tok.type = TOK_DIRECTIVE;
            } 
            else {
                tok.type = TOK_LABEL;
            }

            token_array_push(&tags, tok);
            continue;
        }

        if (*p == '#') {
            p++;
            tok.type = TOK_IMM;
            
            int sign = 1;
            if (*p == '-') { sign = -1; p++; }
            else if (*p == '+') { p++; }

            char *endptr;
            tok.val = strtoll(p, &endptr, 0) * sign;
            if (tok.val > UINT16_MAX) {
                fprintf(stderr, "Warning (Line %d): Immediate value potentially overflows\n", line);
            }
            
            if (p == endptr) {
                fprintf(stderr, "Lexer Error (Line %d): Malformed immediate value\n", line);
                exit(1);
            }
            p = endptr;
            token_array_push(&tags, tok);
            continue;
        }

        if (isalpha(*p) || *p == '_' || *p == '$') {
            int is_explicit_label = (*p == '$');
            int i = 0;
            
            if (is_explicit_label) p++;

            while (*p != '\0' && (isalnum(*p) || *p == '_')) {
                if (i < 31) tok.string[i++] = *p;
                p++;
            }
            tok.string[i] = '\0';

            if (*p == ':') {
                tok.type = TOK_LABEL_DEF;
                p++;
            }
            else if (is_explicit_label) {
                tok.type = TOK_LABEL;
            }
            else if (parse_register(tok.string, &tok.val)) {
                tok.type = TOK_REG;
            }
            else if (is_mnemonic(tok.string)) {
                tok.type = TOK_MNEMONIC;
            }
            else {
                tok.type = TOK_LABEL;
            }

            token_array_push(&tags, tok);
            continue;
        }

        fprintf(stderr, "Lexer Error (Line %d): Unexpected character '%c'\n", line, *p);
        exit(1);
    }

    Token eof_tok = { .type = TOK_EOF, .line_num = line };
    token_array_push(&tags, eof_tok);

    return tags;
}