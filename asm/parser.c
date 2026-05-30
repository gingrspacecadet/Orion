#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cursor = 0;
static token_array *toks;

static Token peek() {
    if (cursor >= toks->len) return toks->data[toks->len - 1];
    return toks->data[cursor];
}

static Token consume() {
    Token t = peek();
    if (t.type != TOK_EOF) cursor++;
    return t;
}

static Token expect(TokenType type, const char *err_msg) {
    Token t = consume();
    if (t.type != type) {
        fprintf(stderr, "Parse Error (Line %d): %s\n", t.line_num, err_msg);
        exit(1);
    }
    return t;
}

static Operand parse_operand() {
    Operand op = {0};
    Token t = peek();

    if (t.type == TOK_REG) {
        consume();
        op.mode = AM_REG;
        op.reg = t.val;
        return op;
    }

    if (t.type == TOK_IMM) {
        consume();
        op.mode = AM_IMM;
        op.val = t.val;
        return op;
    }

    if (t.type == TOK_LABEL || t.type == TOK_DIRECTIVE) {
        consume();
        op.mode = AM_LABEL;
        strncpy(op.label, t.string, 31);
        return op;
    }

    if (t.type == TOK_LBRACE) {
        consume();
        op.mode = AM_REGLIST;
        uint32_t mask = 0;

        while (peek().type != TOK_RBRACE) {
            Token r = expect(TOK_REG, "Expected register in list");
            mask |= (1u << r.val);
            if (peek().type == TOK_COMMA) consume();
        }
        consume();
        op.val = mask;
        return op;
    }

    if (t.type == TOK_LBRACK) {
        consume();
        op.mode = AM_MEM;

        Token base = expect(TOK_REG, "Expected base register after '['");
        op.reg = base.val;
        if (peek().type == TOK_PLUS || peek().type == TOK_MINUS) {
            Token sign_tok = consume();
            int sign = (sign_tok.type == TOK_PLUS) ? 1 : -1;
            Token offset = expect(TOK_IMM, "Expected immediate offset");
            op.is_reg_offset = false;
            op.val = offset.val * sign;
        }

        expect(TOK_RBRACK, "Expected ']' to close memory operand");
        return op;
    }

    fprintf(stderr, "Parse Error (Line %d): Unrecognised operand format\n", t.line_num);
    exit(1);
}

instr_array parse(token_array *tokens) {
    toks = tokens;
    cursor = 0;

    instr_array ast = instr_array_init();

    while (peek().type != TOK_EOF) {
        ParsedInstr instr = {0};
        instr.line_num = peek().line_num;

        Token t = consume();

        if (t.type == TOK_LABEL_DEF) {
            instr.is_label_def = true;
            strncpy(instr.mnemonic, t.string, 31);
            instr_array_push(&ast, instr);
            continue;
        }

        if (t.type == TOK_DIRECTIVE) {
            instr.is_directive = true;
            strncpy(instr.mnemonic, t.string, 31);

            if (peek().type != TOK_EOF && peek().type != TOK_MNEMONIC && peek().type != TOK_LABEL_DEF) {
                instr.ops[instr.op_count++] = parse_operand();
            }
            instr_array_push(&ast, instr);
            continue;
        }

        if (t.type == TOK_MNEMONIC) {
            strncpy(instr.mnemonic, t.string, 31);

            while (peek().type != TOK_EOF && peek().type != TOK_MNEMONIC && peek().type != TOK_LABEL_DEF && peek().type != TOK_DIRECTIVE) {
                instr.ops[instr.op_count++] = parse_operand();

                if (peek().type == TOK_COMMA) consume();

                if (instr.op_count > 3) {
                    fprintf(stderr, "Parse Error (Line %d): Too many operands for instruction\n", t.line_num);
                    exit(1);
                }
            }
            instr_array_push(&ast, instr);
            continue;
        }

        fprintf(stderr, "Parse Error (Line %d): Unexpected token at start of statement\n", t.line_num);
        exit(1);
    }

    return ast;
}