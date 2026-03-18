#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>

// preprocessorslop
#define VECTOR_DECLARE(T) \
typedef struct { \
    T *data; \
    size_t idx; \
    size_t cap; \
} T##Vector; \
static inline T##Vector T##Vector_init(void) { \
    T##Vector v = {calloc(8, sizeof(T)), 0, 8}; return v; \
} \
static inline void T##Vector_push(T##Vector *v, T item) { \
    if (v->idx == v->cap) { \
        v->cap = v->cap ? v->cap * 2 : 8; \
        v->data = realloc(v->data, v->cap * sizeof(T)); \
    } \
    v->data[v->idx++] = item; \
} \
static inline void T##Vector_free(T##Vector *v) { \
    free(v->data); \
    v->idx = v->cap = 0; \
} \

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

typedef enum {
    OPC_ADD,
} OpcType;

typedef struct {
    OpcType type;
    char *mnem;
    uint8_t opcode;
} Opc;

Opc opcodes[] = (Opc[]){
    {OPC_ADD, "add", 0},

    // default "bad" token
    {-1, NULL},
};

Opc parse_opc(Token t) {
    for (size_t i = 0; i < sizeof(opcodes) / sizeof(opcodes[0]); i++) {
        if (strcasecmp(t.data, opcodes[i].mnem) == 0) {
            return opcodes[i];
        }
    }

    return opcodes[sizeof(opcodes) / sizeof(opcodes[0])];
}

int64_t decode_lit(char *str) {
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

void assemble(SourceFile src) {
    LabelVector labels = LabelVector_init();
    size_t pos = 0;
    
    Token t = next_token(&src);

    while (t.type != TOKEN_EOF) {

        if (t.type == TOKEN_LABEL) {
            LabelVector_push(&labels, (Label){pos, t.data});
        }
        else if (t.type == TOKEN_OPC) {
            Opc opc = parse_opc(t);
            if (!opc.mnem) {
                fprintf(stderr, "Unknown opcode %s\n", t.data);
                exit(1);
            }
    
            switch (opc.type) {
                case OPC_ADD: {
                    uint8_t opcode = opc.opcode;
                    t = next_token(&src);
                    if (t.type != TOKEN_REG) {
                        fprintf(stderr, "Expected a register: %s\n", t.data, t.type);
                        exit(1);
                    }
                    uint8_t rn = strtoll(t.data, NULL, 10); //TODO: more bases
                    t = next_token(&src);
                    if (t.type != TOKEN_COMMA) {
                        fprintf(stderr, "Expected a comma: %s", t.data);
                        exit(1);
                    }
                    t = next_token(&src);
                    uint8_t rm = 0;
                    uint32_t imm = 0;
                    if (t.type == TOKEN_REG) {
                        rm = strtoll(t.data, NULL, 10);
                    }
                    else if (t.type == TOKEN_NUM) {
                        imm = decode_lit(t.data);
                    }
                    else {
                        fprintf(stderr, "Unknown token '%s'\n", t.data);
                        exit(1);
                    }

                    t = next_token(&src);
                    if (t.type != TOKEN_NL && t.type != TOKEN_EOF) {
                        fprintf(stderr, "Unexpected token '%s'\n", t.data);
                        exit(1);
                    }
    
                    // this has gotta be wrong
                    uint32_t constructed = 
                        (opcode & 6) << 26 |
                        (rn & 4) << 22 |
                        (rm & 4) << 18 |
                        (imm & 16) << 2 |
                        (imm < 0) << 1 |
                        (imm > (imm < 0 ? INT16_MAX : UINT16_MAX));

                    printf("opc %d rn %d rm %d imm %d\n", opcode, rn, rm, imm);
                }
            }
    
            pos++;
        }
        else if (t.type == TOKEN_NL) {
            t = next_token(&src);
            continue;
        }
        else {
            fprintf(stderr, "Unexpected token '%s'\n", t.data);
            exit(1);
        }
    }

}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Missing source file\n");
        return 1;
    }

    SourceFile src = open_source(argv[1]);

    assemble(src);
}