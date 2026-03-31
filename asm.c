#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>

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

    // Skip whitespace and comments repeatedly
    while (1) {
        if (peek(src) == '\n') {
            charVector_push(&cbuf, consume(src));
            return (Token){TOKEN_NL, cbuf.data};
        }

        while (peek(src) != '\n' && isspace(peek(src)) && peek(src) != EOF)
            consume(src);

        if (peek(src) == ';') {
            while (peek(src) != '\n' && peek(src) != EOF)
                consume(src);
            continue; // loop again to skip newline + more whitespace
        }

        break;
    }


    if (peek(src) == ',') {
        charVector_push(&cbuf, consume(src));
        return (Token){TOKEN_COMMA, cbuf.data};
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

    fprintf(stderr, "Unknown character %c\n", peek(src));
    exit(1);
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
    {OP_MUL, "mul"},
    {OP_DIV, "div"},
    {OP_SHL, "shl"},
    {OP_SHR, "shr"},
    {OP_AND, "and"},
    {OP_OR, "or"},
    {OP_NOT, "not"},
    {OP_XOR, "xor"},
    {OP_MOV, "mov"},
    {OP_LDR, "ldr"},
    {OP_STR, "str"},
    {OP_LDRB, "ldrb"},
    {OP_STRB, "strb"},
    {OP_JXX, "jmp"},     {OP_JXX, "jmpa"},
    {OP_JXX, "jeq"},     {OP_JXX, "jeqa"},
    {OP_JXX, "jne"},     {OP_JXX, "jnea"},
    {OP_JXX, "jlt"},     {OP_JXX, "jlta"},
    {OP_JXX, "jge"},     {OP_JXX, "jgea"},
    {OP_JXX, "jltu"},    {OP_JXX, "jltua"},
    {OP_JXX, "jgeu"},    {OP_JXX, "jgeua"},
    {OP_JXX, "jcs"},     {OP_JXX, "jcsa"},
    {OP_JXX, "jcc"},     {OP_JXX, "jcca"},
    {OP_JXX, "jn"},      {OP_JXX, "jna"},
    {OP_JXX, "jp"},      {OP_JXX, "jpa"},
    {OP_JXX, "jvs"},     {OP_JXX, "jvsa"},
    {OP_JXX, "jvc"},     {OP_JXX, "jvca"},
    {OP_JXX, "jhi"},     {OP_JXX, "jhia"},
    {OP_JXX, "jls"},     {OP_JXX, "jlsa"},
    {OP_CALL, "call"},
    {OP_RET, "ret"},
    {OP_PUSH, "push"},
    {OP_POP, "pop"},
    {OP_INTE, "inte"},

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

uint32_t decode_lit(size_t pos, LabelVector labels, Token t) {
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
            else if (str[1] == '\0') {
                base = 10;
            }
            else {
                fprintf(stderr, "Unknown base for lit %s\n", str);
                exit(1);
            }
        }
        return strtol(str, NULL, base);
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

uint8_t decode_reg(Token t) {
    if (t.type != TOKEN_REG) {
        fprintf(stderr, "Expected a register, got %s\n", t.data);
        exit(1);
    }

    if (strcasecmp(t.data, "sp") == 0) {
        return 15;
    }

    char *endptr;
    errno = 0;
    uint8_t result = strtol(t.data, &endptr, 10);
    if (endptr == t.data) {
        fprintf(stderr, "Invalid register number %s\n", t.data);
        exit(1);
    }
    if ((result == LONG_MAX || result == LONG_MIN) && errno == ERANGE) {
        fprintf(stderr, "Invalid register number %s\n", t.data);
        exit(1);
    }

    if (result < 0 || result > 15) {
        fprintf(stderr, "Invalid register number %s (must be within range 0-15)\n", t.data);
        exit(1);
    }

    return result;
}

Token expect(TokenType type, SourceFile *src, const char *msg, ...) {
    Token t = next_token(src);
    if (t.type != type) {
        va_list args;
        va_start(args, msg);
        fprintf(stderr, "Parse error: ");
        vfprintf(stderr, msg, args);
        putc('\n', stderr);
        exit(1);
    }

    return t;
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
            pos += 4;
        }

        t = next_token(&src);
    }

    // reset source
    src.contents.idx = 0;
    pos = 0;

    memset(&t, 0, sizeof(t));

    while (1) {  
        t = next_token(&src);
        if (t.type == TOKEN_EOF) break;

        if (t.type == TOKEN_LABEL) continue;   // already handled
        if (t.type == TOKEN_NL) continue;
        if (t.type != TOKEN_OPC) {
            fprintf(stderr, "Expected an opcode, got '%s'\n", t.data, t.type);
            exit(1);
        }

        Opc opc = parse_opc(t);
        if (!opc.mnem) {
            fprintf(stderr, "Unknown opcode %s\n", t.data);
            exit(1);
        }

        uint8_t rn = 0;
        uint8_t rd = 0;
        uint8_t rm = 0;
        uint32_t imm = 0;

        int ext = 0;
        int reg = 0;

        uint32_t constructed = 0;
        
        switch (opc.opcode) {
            case OP_ADD:
            case OP_SUB:
            case OP_MOV: {
                t = expect(TOKEN_REG, &src, "Expected the target register, got %s", t.data);
                rn = decode_reg(t);
                t = expect(TOKEN_COMMA, &src, "Expected a comma, got %s", t.data);
                t = next_token(&src);
                if (t.type == TOKEN_REG) {
                    reg = 1;
                    rm = decode_reg(t);
                }
                else if (t.type == TOKEN_NUM || t.type == TOKEN_REF) {
                    imm = decode_lit(pos, labels, t);
                }
                else {
                    fprintf(stderr, "Expected an argument register, number, or reference, got %s\n", t.data);
                    exit(1);
                }

                t = next_token(&src);
                if (t.type == TOKEN_COMMA) {
                    t = expect(TOKEN_REG, &src, "Expected destination register, got %s", t.data);
                    rd = decode_reg(t);
                    t = next_token(&src);
                } else {
                    rd = rn;
                }

                if (t.type != TOKEN_NL && t.type != TOKEN_EOF) {
                    fprintf(stderr, "Expected a newline, got '%s'\n", t.data, t.type);
                    exit(1);
                }

                if (imm > UINT16_MAX || (int32_t)imm < 0) {
                    ext = 1;
                }

                constructed = 
                    (opc.opcode & 0x3F) << 26 |
                    (rn & 0xF) << 22 |
                    (rd & 0xF) << 18 |
                    (rm & 0xF) << 14 |
                    (imm & 0xFFFF) << 2 |
                    (reg) << 1 |
                    (ext);
                
                break;
            }

            case OP_JXX: {
                t = next_token(&src);
                if (t.type == TOKEN_REF || t.type == TOKEN_NUM) {
                    imm = decode_lit(pos, labels, t);
                } else if (t.type == TOKEN_REG) {
                    rm = decode_reg(t);
                } else {
                    fprintf(stderr, "Expected an argument register, number, or reference, got %s\n", t.data);
                    exit(1);
                }

                if (imm > UINT16_MAX || (int32_t)imm < 0) {
                    ext = 1;
                }

                uint8_t cond = 0;
                uint8_t absolute = 0;
                char *mnem = strdup(opc.mnem + 1);
                if (mnem[strlen(mnem) - 1] == 'a') {
                    absolute = 1;
                    mnem[strlen(mnem) - 1] = '\0';
                }

                if (strcasecmp(mnem, "mp") == 0) cond = 0x0;
                else if (strcasecmp(mnem, "eq") == 0) cond = 0x1;
                else if (strcasecmp(mnem, "ne") == 0) cond = 0x2;
                else if (strcasecmp(mnem, "lt") == 0) cond = 0x3;
                else if (strcasecmp(mnem, "ge") == 0) cond = 0x4;
                else if (strcasecmp(mnem, "ltu") == 0) cond = 0x5;
                else if (strcasecmp(mnem, "geu") == 0) cond = 0x6;
                else if (strcasecmp(mnem, "cs") == 0) cond = 0x7;
                else if (strcasecmp(mnem, "cc") == 0) cond = 0x8;
                else if (strcasecmp(mnem, "n") == 0) cond = 0x9;
                else if (strcasecmp(mnem, "p") == 0) cond = 0xA;
                else if (strcasecmp(mnem, "vs") == 0) cond = 0xB;
                else if (strcasecmp(mnem, "vc") == 0) cond = 0xC;
                else if (strcasecmp(mnem, "hi") == 0) cond = 0xD;
                else if (strcasecmp(mnem, "ls") == 0) cond = 0xE;
                // else if (strcmp(mnem, "reserved") == 0) cond = 0xF;
                else {
                    fprintf(stderr, "Unknown opcode %s\n", opc.opcode);
                    exit(1);
                }

                constructed = 
                    (opc.opcode & 0x3F) << 26 |
                    (cond & 0xF) << 22 |
                    (absolute & 0x1) << 21 |
                    // reserved << 18 |
                    (rm & 0xF) << 14 |
                    (imm & 0xFFFF) << 2 |
                    (reg & 0x1) << 1 |
                    (ext & 0x1);

                break;
            }

            default: {
                fprintf(stderr, "Unknown opcode %s\n", opc.mnem);
                exit(1);
            }
        }

        fwrite(&constructed, sizeof(uint32_t), 1, out);

        if (ext) {
            fwrite(&imm, sizeof(uint32_t), 1, out);
            pos += 4;
        }

        pos += 4;
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
