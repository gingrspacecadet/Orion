#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>
#include "opcodes.h"

#include "vector.h"

typedef enum {
    TOKEN_LABEL,
    TOKEN_OPC,
    TOKEN_REF,
    TOKEN_NUM,
    TOKEN_REG,
    TOKEN_NL,
    TOKEN_COMMA,
    TOKEN_EOF,
} TokenType;

typedef struct {
    TokenType type;
    char *data;
} Token;

VECTOR_DECLARE(char);

typedef struct {
    charVector contents;
    size_t length;
} SourceFile;

SourceFile open_source(char *path) {
    if (!path) exit(1);

    FILE *fptr = fopen(path, "r");
    if (!fptr) {
        fprintf(stderr, "Failed to open source file %s\n", path);
        exit(1);
    }
    fseek(fptr, 0, SEEK_END);
    size_t flen = ftell(fptr);
    fseek(fptr, 0, SEEK_SET);
    char *fbuf = malloc(flen);
    if (!fbuf) {
        fprintf(stderr, "Failed to allocate source buffer\n");
        exit(1);
    }
    if (fread(fbuf, 1, flen, fptr) != flen) {
        fprintf(stderr, "Failed to read source file %s\n", path);
        exit(1);
    }
    fclose(fptr);

    SourceFile s = { charVector_init(), flen};
    s.contents.data = realloc(s.contents.data, flen);
    memcpy(s.contents.data, fbuf, flen);
    s.contents.cap = flen;
    s.contents.idx = 0;

    return s;
}

char peek(SourceFile *src) {
    if (src->contents.idx >= src->contents.cap) return EOF;
    return src->contents.data[src->contents.idx];
}

char consume(SourceFile *src) {
    return src->contents.data[src->contents.idx++];
}

Token next_token(SourceFile *src) {
    // shouldn't ever happen :/
    if (!src->contents.data) {
        fprintf(stderr, "Unexpected end of file stream\n");
        exit(1);
    }

    charVector cbuf = charVector_init();

    if (peek(src) == '\n') {
        charVector_push(&cbuf, consume(src));
        return (Token){TOKEN_NL, cbuf.data};
    }

    if (peek(src) == ',') {
        charVector_push(&cbuf, consume(src));
        return (Token){TOKEN_COMMA, cbuf.data};
    }

    if (isspace(peek(src))) {
        while (isspace(peek(src)) && peek(src) != EOF) {
            consume(src);
        }
    }

    // Literals start with '#'
    if (peek(src) == '#') {
        consume(src);
        if (peek(src) == '-') charVector_push(&cbuf, consume(src));
        while ((isxdigit(peek(src)) || tolower(peek(src)) == 'x' || tolower(peek(src)) == 'b' || tolower(peek(src)) == 'o') && peek(src) != EOF) {
            charVector_push(&cbuf, consume(src));
        }
        return (Token){TOKEN_NUM, cbuf.data};
    }

    // Label references start with '$'
    if (peek(src) == '$') {
        consume(src);
        while ((isalpha(peek(src)) || peek(src) == '_') && peek(src) != EOF) {
            charVector_push(&cbuf, consume(src));
        }
        return (Token){TOKEN_REF, cbuf.data};
    }

    // Registers start with 'R'
    if (peek(src) == 'R' || peek(src) == 'r') {
        consume(src);
        while (isdigit(peek(src)) && peek(src) != EOF) {
            charVector_push(&cbuf, consume(src));
        }
        return (Token){TOKEN_REG, cbuf.data};
    }

    if (isalpha(peek(src)) || peek(src) == '_') {
        while ((isalpha(peek(src)) || peek(src) == '_') && peek(src) != EOF) {
            charVector_push(&cbuf, consume(src));
        }
        if (peek(src) == ':') {
            consume(src);
            return (Token){TOKEN_LABEL, cbuf.data};
        } else {
            return (Token){TOKEN_OPC, cbuf.data};
        }
    }

    if (peek(src) == EOF) {
        return (Token){TOKEN_EOF, cbuf.data};
    }
}

typedef struct {
    size_t pos;
    char *name;
} Label;

VECTOR_DECLARE(Label);

typedef struct {
    uint8_t opcode;
    char *mnem;
} Opc;

Opc opcodes[] = (Opc[]){
    {OP_ADD, "add"},
    {OP_SUB, "sub"},
    {OP_MOV, "mov"},

    // default "bad" token
    {-1, NULL},
};

Opc parse_opc(Token t) {
    for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); i++) {
        if (!t.data || !opcodes[i].mnem) continue;
        if (strcasecmp(t.data, opcodes[i].mnem) == 0) {
            return opcodes[i];
        }
    }

    // the last opcode
    return opcodes[sizeof(opcodes) / sizeof(opcodes[0])];
}

int64_t decode_lit(size_t pos, LabelVector labels, Token t) {
    if (t.type == TOKEN_NUM) {
        char *str = t.data;
        uint8_t base = 10;
        if (str[0] == '0') {
            if (tolower(str[1]) == 'x') {
                base = 16;
            }
            else if (tolower(str[1]) == 'o') {
                base = 8;
            } 
            else if (tolower(str[1]) == 'b') {
                base = 2;
            }
            else {
                fprintf(stderr, "Unknown base for lit %s\n", str);
                exit(1);
            }
        }
        return strtoll(str, NULL, base);
    }
    else if (t.type == TOKEN_REF) {
        for (size_t i = 0; i < labels.idx; i++) {
            if (!labels.data[i].name) continue;
            if (strcmp(labels.data[i].name, t.data) == 0) {
                return labels.data[i].pos - pos;
            }
        }

        fprintf(stderr, "Unknown label reference %s\n", t.data);
        exit(1);
    }
    else {
        fprintf(stderr, "Unknown literal %s\n", t.data);
        exit(1);
    }
}

void assemble(SourceFile src, FILE *out) {
    LabelVector labels = LabelVector_init();
    size_t pos = 0;
    
    Token t = next_token(&src);

    // first pass - collect labels
    while (t.type != TOKEN_EOF) {
        if (t.type == TOKEN_LABEL) {
            LabelVector_push(&labels, (Label){pos, t.data});
        }
        else if (t.type == TOKEN_OPC) {
            pos++;
        }

        t = next_token(&src);
    }

    // reset source
    src.contents.idx = 0;
    pos = 0;

    t = next_token(&src);

    while (t.type != TOKEN_EOF) {
        if (t.type == TOKEN_LABEL) {}   // already handled
        else if (t.type == TOKEN_OPC) {
            Opc opc = parse_opc(t);
            if (!opc.mnem) {
                fprintf(stderr, "Unknown opcode %s\n", t.data);
                exit(1);
            }
            
            switch (opc.opcode) {
                case OP_ADD:
                case OP_SUB:
                case OP_MOV: {
                    t = next_token(&src);
                    if (t.type != TOKEN_REG) {
                        fprintf(stderr, "Expected a register: %s\n", t.data, t.type);
                        exit(1);
                    }
                    uint8_t rn = strtoll(t.data, NULL, 10);
                    t = next_token(&src);
                    if (t.type != TOKEN_COMMA) {
                        fprintf(stderr, "Expected a comma: %s", t.data);
                        exit(1);
                    }
                    t = next_token(&src);
                    uint8_t rm = 0;
                    int64_t imm = 0;
                    if (t.type == TOKEN_REG) {
                        rm = strtol(t.data, NULL, 10);
                    }
                    else if (t.type == TOKEN_NUM || t.type == TOKEN_REF) {
                        imm = decode_lit(pos, labels, t);
                    }
                    else {
                        fprintf(stderr, "Unknown token '%s' (%d)\n", t.data, t.type);
                        exit(1);
                    }
                    
                    t = next_token(&src);
                    if (t.type != TOKEN_NL && t.type != TOKEN_EOF) {
                        fprintf(stderr, "Unexpected token '%s' (%d)\n", t.data, t.type);
                        exit(1);
                    }

                    int sign = (imm < 0);
                    int ext = (sign ? (imm < INT16_MIN) : (imm > UINT16_MAX));
                    
                    uint32_t constructed = 
                    (opc.opcode & 0x3F) << 26 |
                    (rn & 0xF) << 22 |
                    (rm & 0xF) << 18 |
                    (imm & 0xFFFF) << 2 |
                    (sign) << 1 |
                    (ext);

                    fwrite(&constructed, 4, 1, out);

                    if (ext) fwrite(&imm, 4, 1, out);
                    
                    break;
                }
                
                default: {
                    fprintf(stderr, "Unknown opcode %s\n", opc.mnem);
                    exit(1);
                }
            }
            
            pos++;
        }
        else if (t.type == TOKEN_NL){}    // skip empty lines
        else {
            fprintf(stderr, "Unexpected token '%s' (%d)\n", t.data, t.type);
            exit(1);
        }

        t = next_token(&src);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Missing source file\n");
        return 1;
    }

    SourceFile src = open_source(argv[1]);

    char *outpath = "a.out";
    if (argc >= 3) outpath = argv[2];

    assemble(src, fopen(outpath, "wb"));
}
