#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "parser.h"
#include "token.h"
#include "source.h"
#include "lexer.h"

static inline Token peek(Source *source) {
    ptrdiff_t old = source->offset;
    Token t = lex(source);
    source->offset = old;
    return t;
}

static inline void advance(Source *source) {
    lex(source);
}

static inline Token consume(Source *source) {
    return lex(source);
}

Operand parse_operand(Source *source) {
    Operand out = {0};
    Token t = peek(source);

    switch (t.type) {
        case TK_REG: {
            advance(source);
            out.mode = AM_REG;
            out.reg = t.val;
            break;
        }

        case TK_IMM: {
            advance(source);
            out.mode = AM_IMM;
            out.val = t.val;
            break;
        }

        case TK_REF:
        case TK_MNEM:
        case TK_DIRECTIVE: {
            advance(source);
            out.mode = AM_REF;
            strncpy(out.label, t.string, 31);
            break;
        }

        case TK_LBRACE: {
            advance(source);
            out.mode = AM_REGLIST;
            uint16_t mask = 0;

            while (peek(source).type != TK_RBRACE) {
                Token r = consume(source);
                if (r.type != TK_REG) {
                    fprintf(stderr, "Assembler error (line %d): Expected a register in list\n", r.line_num);
                    continue;
                }
                mask |= (1u << r.val);
                if (peek(source).type == TK_COMMA) advance(source);
            }
            advance(source);
            out.val = mask;
            break;
        }

        case TK_LBRACK: {
            advance(source);
            out.mode = AM_MEM;

            Token base = consume(source);
            if (base.type != TK_REG) {
                fprintf(stderr, "Assembler Error (line %d): Expected base register after '['\n", base.line_num);
                base.val = 0;
            }

            out.reg = base.val;
            if (peek(source).type == TK_PLUS || peek(source).type == TK_MINUS) {
                Token sign_tk = consume(source);
                int sign = (sign_tk.type == TK_PLUS) ? 1 : -1;
                Token offset = consume(source);
                if (offset.type != TK_IMM) {
                    fprintf(stderr, "Assembler Error (line %d): Expected immediate offset\n", offset.type);
                    offset.val = 0;
                }
                out.is_reg_offset = false;
                out.val = offset.val * sign;
            }

            t = consume(source);
            if (t.type != TK_RBRACK) {
                fprintf(stderr, "Assembler Error (line %d): Expected ']' to close memory operand\n", t.line_num);
            }

            break;
        }

        default: {
            fprintf(stderr, "Assembler Error (line %d): Unrecognised operand format\n", t.line_num);
        }
    }

    return out;
}

Instr parse(Source *source) {
    Instr out = {.line_num = peek(source).line_num};
    Token t = lex(source);
    
    switch (t.type) {
        case TK_LABEL: {
            out.is_label = true;
            strncpy(out.name, t.string, 31);
            break;
        }

        case TK_MNEM: 
        case TK_DIRECTIVE: {
            out.is_directive = (t.type == TK_DIRECTIVE);
            uint8_t cap = 2;
            strncpy(out.name, t.string, 31);
            while (peek(source).line_num == t.line_num) {
                if (!out.ops)
                    out.ops = malloc(sizeof(Operand) * cap);

                if (out.op_count >= cap) {
                    cap *= 2;
                    out.ops = realloc(out.ops, sizeof(Operand) * cap);
                }

                out.ops[out.op_count++] = parse_operand(source);

                if (peek(source).type == TK_COMMA) advance(source);
                else break;
            }

            break;
        }

        case TK_EOF: {
            return (Instr){.is_eof = true};
        }

        default: {
            fprintf(stderr, "Assembler Error (line %u): Unexpected token %s\n", t.line_num, token_names[t.type]);
            advance(source);
        }
    }

    return out;
}
