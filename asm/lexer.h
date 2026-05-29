#ifndef LEXER_H
#define LEXER_H

#include "gin.h"

typedef enum {
    TOK_MNEMONIC,   // "mov", "add", "jeq"
    TOK_REG,        // r0, rsp
    TOK_IMM,        // #4, #0x10, #-5
    TOK_LABEL_DEF,  // main:
    TOK_LABEL,      // $main
    TOK_COMMA,
    TOK_LBRACK,
    TOK_RBRACK,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_PLUS,
    TOK_MINUS,
    TOK_DIRECTIVE,  // .org, .word
    TOK_EOF
} TokenType;

typedef struct {
    TokenType type;
    int64_t val;
    char string[32];
    int line_num;
} Token;

INSTANTIATE(Token, token, ARRAY_TEMPLATE)

token_array lex(char *source);

#endif