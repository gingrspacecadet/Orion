#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include "lexer.h"
#include "source.h"

static bool parse_reg(char string[32], int64_t *out_val) {
    if (string[0] != 'r' && string[0] != 'R')
        return false;

    string++;

    if (strcasecmp(string, "sp") == 0) {
        *out_val = 15;
        return true;
    }
    
    char *endptr;
    *out_val = strtol(string, &endptr, 0);
    if (string == endptr) {
        return false;
    }
    
    return true;
}

static inline char peek(Source *source) {
    return source->data[source->offset];
}

static inline void advance(Source *source) {
    source->offset++;
}

static inline char consume(Source *source) {
    char c = peek(source);
    advance(source);
    return c;
}

Token lex(Source *source) {
    Token t = {.line_num = source->line, .type = TK_EOF};

    while (peek(source) != '\0') {
        if (peek(source) == ' ' || peek(source) == '\t' || peek(source) == '\r') {
            advance(source);
            continue;
        }

        if (peek(source) == '\n') {
            source->line++;
            advance(source);
            continue;
        }

        if (peek(source) == ';') {
            while (peek(source) != '\0' && peek(source) != '\n') {
                advance(source);
            }
            continue;
        }

        switch (peek(source)) {
            case ',': t.type = TK_COMMA; advance(source); break;
            case '[': t.type = TK_LBRACK; advance(source); break;
            case ']': t.type = TK_RBRACK; advance(source); break;
            case '{': t.type = TK_LBRACE; advance(source); break;
            case '}': t.type = TK_RBRACE; advance(source); break;
            case '+': t.type = TK_PLUS; advance(source); break;
            case '-': t.type = TK_MINUS; advance(source); break;

            case '.': {
                int i = 0;
                t.string[i++] = consume(source);

                while (peek(source) != '\0' && (isalnum(peek(source)) || peek(source) == '_')) {
                    if (i < 31) t.string[i++] = consume(source);
                    else fprintf(stderr, "Assembler Error (line %zu): Label string too long\n", source->line);
                }
                t.string[i] = '\0';

                if (peek(source) == ':') {
                    t.type = TK_LABEL;
                    advance(source);
                } else {
                    t.type = TK_DIRECTIVE;
                }

                break;
            }

            case '#': {
                advance(source);
                t.type = TK_IMM;

                int sign = 1;
                if (peek(source) == '-') {sign = -1; advance(source);}
                else if (peek(source) == '+') advance(source);

                char *endptr;
                t.val = strtoll(&source->data[source->offset], &endptr, 0) * sign;
                
                if (&source->data[source->offset] == endptr) {
                    fprintf(stderr, "Assembler Error (line %zu): Malformed immediate value\n", source->line);
                }

                source->offset += endptr - &source->data[source->offset];
                break;
            }

            default: {
                if (isalpha(peek(source)) || peek(source) == '_' || peek(source) == '$') {
                    bool is_ref = (peek(source) == '$');
                    int i = 0;

                    if (is_ref) advance(source);

                    while (peek(source) != '\0' && (isalnum(peek(source)) || peek(source) == '_')) {
                        if (i < 31) t.string[i++] = peek(source);
                        else fprintf(stderr, "Assembler Error (line %zu): String too long\n", source->line);
                        advance(source);
                    }
                    t.string[i] = '\0';

                    if (peek(source) == ':') {
                        t.type = TK_LABEL;
                        advance(source);
                    } else if (is_ref) {
                        t.type = TK_REF;
                    } else if (parse_reg(t.string, &t.val)) {
                        t.type = TK_REG;
                    } else {
                        t.type = TK_MNEM;
                    }

                    break;
                }

                fprintf(stderr, "Assembler Error (line %zu): Unexpected character %c\n", source->line, peek(source));
            }
        }

        break;
    }
    
    return t;
}