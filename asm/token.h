#ifndef TOKEN_H
#define TOKEN_H

#include <stdint.h>

#define X_TOKENTYPE \
    X(MNEM) \
    X(REG) \
    X(IMM) \
    X(LABEL) \
    X(REF) \
    X(COMMA) \
    X(LBRACK) \
    X(RBRACK) \
    X(LBRACE) \
    X(RBRACE) \
    X(PLUS) \
    X(MINUS) \
    X(DIRECTIVE) \
    X(NEWLINE) \
    X(EOF)

#define X(name) TK_##name,
typedef enum {
    X_TOKENTYPE
} TokenType;
#undef X

extern const char *token_names[];

typedef struct {
    TokenType type;
    int64_t val;
    char string[32];
    int line_num;
} Token;

#endif