#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include "token.h"
#include "source.h"

Token lex(Source *source);

#endif