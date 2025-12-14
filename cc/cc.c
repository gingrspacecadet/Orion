/*
 * Orion C compiler
 * Supports:
 *  - Function definitions: `int name() { ... }` and `int name(int a, int b) { ... }`
 *  - Local variable declarations: `int x = 5;`
 *  - Assignments: `x = expr;`
 *  - Expressions: +, -, *, /, %, ==, !=, <, >, <=, >=, &&, ||
 *  - Control flow: `if`, `else`, `while`, `for`
 *  - `return` statements
 *  - Function calls: `foo()` or `foo(a, b)`
 *
 * Calling convention:
 *  - Arguments passed in r0, r1, r2, r3
 *  - Return value in r0
 *  - Stack used for local variables (r15 = stack pointer)
 *  - Callee-saved: r4-r14
 *
 * Build: gcc -O2 -o cc/cc cc/cc.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

typedef enum {
    TK_IDENT, TK_NUM, TK_KW_INT, TK_KW_RETURN, TK_KW_IF, TK_KW_ELSE,
    TK_KW_WHILE, TK_KW_FOR, TK_KW_ASM, TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_SEMI, TK_COMMA, TK_ASSIGN, TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH,
    TK_PERCENT, TK_EQ, TK_NE, TK_LT, TK_GT, TK_LE, TK_GE, TK_AND, TK_OR,
    TK_STRING, TK_EOF
} TokenKind;

typedef struct { TokenKind kind; char *text; long val; } Token;

const char *src;
size_t pos;
Token curtok;
FILE *out_file;
char** functions;
int function_counter = 0;
int label_counter = 0;

char *gen_label() {
    static char buf[32];
    snprintf(buf, sizeof(buf), ".L%d", label_counter++);
    return strdup(buf);
}

void emit(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out_file, fmt, ap);
    va_end(ap);
}

void next_token() {
    while (isspace((unsigned char)src[pos])) pos++;
    if (src[pos] == '\0') { curtok.kind = TK_EOF; curtok.text = NULL; return; }

    if (isalpha((unsigned char)src[pos]) || src[pos]=='_') {
        size_t start = pos;
        while (isalnum((unsigned char)src[pos]) || src[pos]=='_') pos++;
        size_t len = pos - start;
        char *s = strndup(src + start, len);
        if (strcmp(s, "int") == 0) curtok.kind = TK_KW_INT;
        else if (strcmp(s, "return") == 0) curtok.kind = TK_KW_RETURN;
        else if (strcmp(s, "if") == 0) curtok.kind = TK_KW_IF;
        else if (strcmp(s, "else") == 0) curtok.kind = TK_KW_ELSE;
        else if (strcmp(s, "while") == 0) curtok.kind = TK_KW_WHILE;
        else if (strcmp(s, "for") == 0) curtok.kind = TK_KW_FOR;
        else if (strcmp(s, "asm") == 0) curtok.kind = TK_KW_ASM;
        else curtok.kind = TK_IDENT;
        curtok.text = s;
        return;
    }

    if (isdigit((unsigned char)src[pos])) {
        long v = 0;
        int base = 10;
        if (src[pos]=='0' && (src[pos+1]=='x' || src[pos+1]=='X')) { base = 16; pos += 2; }
        while (isxdigit((unsigned char)src[pos])) {
            char c = src[pos++];
            int d;
            if (isdigit((unsigned char)c)) d = c - '0';
            else if (c>='a' && c<='f') d = 10 + c - 'a';
            else d = 10 + c - 'A';
            v = v*base + d;
        }
        curtok.kind = TK_NUM;
        curtok.val = v;
        curtok.text = NULL;
        return;
    }

    if (src[pos] == '"') {
        pos++;  /* skip opening quote */
        size_t bufcap = 64;
        size_t buflen = 0;
        char *buf = malloc(bufcap);
        if (!buf) { perror("malloc"); exit(1); }
        while (src[pos] != '"' && src[pos] != '\0') {
            char ch = src[pos++];
            if (ch == '\\') {
                if (src[pos] == '\0') break;
                char esc = src[pos++];
                unsigned int val = 0;
                if (esc == 'n') ch = '\n';
                else if (esc == 't') ch = '\t';
                else if (esc == 'r') ch = '\r';
                else if (esc == '\\') ch = '\\';
                else if (esc == '\'') ch = '\''; 
                else if (esc == '"') ch = '"';
                else if (esc == 'x') {
                    /* hex escape: one or more hex digits (limit to two for a byte) */
                    int cnt = 0; val = 0;
                    while (isxdigit((unsigned char)src[pos]) && cnt < 2) {
                        char h = src[pos++];
                        val = val * 16 + (isdigit((unsigned char)h) ? h - '0' : (h >= 'a' ? 10 + h - 'a' : 10 + h - 'A'));
                        cnt++;
                    }
                    ch = (char)val;
                } else if (esc >= '0' && esc <= '7') {
                    /* octal escape: up to 3 digits total (we already consumed one) */
                    val = esc - '0';
                    int cnt = 1;
                    while (cnt < 3 && src[pos] >= '0' && src[pos] <= '7') {
                        val = val * 8 + (src[pos++] - '0');
                        cnt++;
                    }
                    ch = (char)val;
                } else {
                    /* unknown escape: treat literally */
                    ch = esc;
                }
            }
            /* append ch to buffer */
            if (buflen + 1 >= bufcap) {
                bufcap *= 2;
                buf = realloc(buf, bufcap);
                if (!buf) { perror("realloc"); exit(1); }
            }
            buf[buflen++] = ch;
        }
        if (src[pos] != '"') { fprintf(stderr, "Unterminated string\n"); exit(1); }
        pos++;  /* skip closing quote */
        buf[buflen] = '\0';
        curtok.kind = TK_STRING;
        curtok.text = buf;
        return;
    }

    char c = src[pos++];
    switch (c) {
        case '(': curtok.kind = TK_LPAREN; break;
        case ')': curtok.kind = TK_RPAREN; break;
        case '{': curtok.kind = TK_LBRACE; break;
        case '}': curtok.kind = TK_RBRACE; break;
        case ';': curtok.kind = TK_SEMI; break;
        case ',': curtok.kind = TK_COMMA; break;
        case '+': curtok.kind = TK_PLUS; break;
        case '-': curtok.kind = TK_MINUS; break;
        case '*': curtok.kind = TK_STAR; break;
        case '/': curtok.kind = TK_SLASH; break;
        case '%': curtok.kind = TK_PERCENT; break;
        case '=':
            if (src[pos] == '=') { pos++; curtok.kind = TK_EQ; }
            else curtok.kind = TK_ASSIGN;
            break;
        case '!':
            if (src[pos] == '=') { pos++; curtok.kind = TK_NE; }
            else { fprintf(stderr, "Unexpected character: !\n"); exit(1); }
            break;
        case '<':
            if (src[pos] == '=') { pos++; curtok.kind = TK_LE; }
            else curtok.kind = TK_LT;
            break;
        case '>':
            if (src[pos] == '=') { pos++; curtok.kind = TK_GE; }
            else curtok.kind = TK_GT;
            break;
        case '&':
            if (src[pos] == '&') { pos++; curtok.kind = TK_AND; }
            else { fprintf(stderr, "Unexpected character: &\n"); exit(1); }
            break;
        case '|':
            if (src[pos] == '|') { pos++; curtok.kind = TK_OR; }
            else { fprintf(stderr, "Unexpected character: |\n"); exit(1); }
            break;
        default:
            fprintf(stderr, "Unexpected character: %c\n", c);
            exit(1);
    }
    curtok.text = NULL;
}


void expect(TokenKind k) {
    if (curtok.kind != k) { fprintf(stderr, "Unexpected token (expected %d, got %d)\n", k, curtok.kind); exit(1); }
    next_token();
}

/* Scope tracking for variables */
typedef struct Var {
    char *name;
    int offset;  /* stack offset (in units of words) */
    struct Var *next;
} Var;

typedef struct {
    Var *vars;
    int stack_depth;
    int arg_count;
} Scope;

Scope *scope = NULL;

void scope_enter(int arg_count) {
    Scope *s = malloc(sizeof(*s));
    s->vars = NULL;
    s->stack_depth = arg_count;
    s->arg_count = arg_count;
    scope = s;
}

void scope_exit() {
    Scope *s = scope;
    scope = NULL;
    /* Free vars */
    while (s->vars) {
        Var *v = s->vars;
        s->vars = v->next;
        free(v->name);
        free(v);
    }
    free(s);
}

Var *scope_find(const char *name) {
    if (!scope) return NULL;
    for (Var *v = scope->vars; v; v = v->next)
        if (strcmp(v->name, name) == 0) return v;
    return NULL;
}

void scope_add_var(const char *name) {
    if (scope_find(name)) { fprintf(stderr, "Duplicate variable: %s\n", name); exit(1); }
    Var *v = malloc(sizeof(*v));
    v->name = strdup(name);
    v->offset = scope->stack_depth++;
    v->next = scope->vars;
    scope->vars = v;
}

int scope_get_var_offset(const char *name) {
    Var *v = scope_find(name);
    if (!v) { fprintf(stderr, "Undefined variable: %s\n", name); exit(1); }
    return v->offset;
}

/* AST */
typedef enum {
    ND_NUM, ND_VAR, ND_CALL, ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD,
    ND_EQ, ND_NE, ND_LT, ND_GT, ND_LE, ND_GE, ND_AND, ND_OR,
    ND_ASSIGN, ND_STMT_LIST, ND_IF, ND_WHILE, ND_FOR, ND_RETURN, ND_ASM
} NodeKind;

typedef struct Node {
    NodeKind kind;
    long val;
    char *name;
    struct Node *lhs, *rhs, *cond, *body, *els, *init, *incr, *next;
} Node;

Node *new_num(long v) { Node *n = calloc(1,sizeof(*n)); n->kind = ND_NUM; n->val = v; return n; }
Node *new_var(const char *name) { Node *n = calloc(1,sizeof(*n)); n->kind = ND_VAR; n->name = strdup(name); return n; }
Node *new_call(char *name) { Node *n = calloc(1,sizeof(*n)); n->kind = ND_CALL; n->name = name; return n; }
Node *new_bin(NodeKind k, Node *l, Node *r) { Node *n = calloc(1,sizeof(*n)); n->kind = k; n->lhs = l; n->rhs = r; return n; }
Node *new_stmt(NodeKind k) { Node *n = calloc(1,sizeof(*n)); n->kind = k; return n; }

/* Forward declare parser functions */
Node *parse_stmt();
Node *parse_expr();

Node *parse_primary() {
    if (curtok.kind == TK_NUM) {
        Node *n = new_num(curtok.val);
        next_token();
        return n;
    }
    if (curtok.kind == TK_IDENT) {
        char *name = strdup(curtok.text); next_token();
        if (curtok.kind == TK_LPAREN) {
            next_token();
            Node *call = new_call(name);
            if (curtok.kind != TK_RPAREN) {
                /* Parse argument list - for now just return first arg */
                call->lhs = parse_expr();
                while (curtok.kind == TK_COMMA) {
                    next_token();
                    call->rhs = parse_expr();
                }
            }
            expect(TK_RPAREN);
            return call;
        }
        /* It's a variable */
        return new_var(name);
    }
    if (curtok.kind == TK_LPAREN) {
        next_token();
        Node *n = parse_expr();
        expect(TK_RPAREN);
        return n;
    }
    fprintf(stderr, "Unexpected token in primary\n"); exit(1);
}

Node *parse_mul() {
    Node *n = parse_primary();
    while (curtok.kind == TK_STAR || curtok.kind == TK_SLASH || curtok.kind == TK_PERCENT) {
        if (curtok.kind == TK_STAR) { next_token(); n = new_bin(ND_MUL, n, parse_primary()); }
        else if (curtok.kind == TK_SLASH) { next_token(); n = new_bin(ND_DIV, n, parse_primary()); }
        else { next_token(); n = new_bin(ND_MOD, n, parse_primary()); }
    }
    return n;
}

Node *parse_add() {
    Node *n = parse_mul();
    while (curtok.kind == TK_PLUS || curtok.kind == TK_MINUS) {
        if (curtok.kind == TK_PLUS) { next_token(); n = new_bin(ND_ADD, n, parse_mul()); }
        else { next_token(); n = new_bin(ND_SUB, n, parse_mul()); }
    }
    return n;
}

Node *parse_cmp() {
    Node *n = parse_add();
    while (curtok.kind == TK_LT || curtok.kind == TK_GT || curtok.kind == TK_LE || curtok.kind == TK_GE || curtok.kind == TK_EQ || curtok.kind == TK_NE) {
        if (curtok.kind == TK_LT) { next_token(); n = new_bin(ND_LT, n, parse_add()); }
        else if (curtok.kind == TK_GT) { next_token(); n = new_bin(ND_GT, n, parse_add()); }
        else if (curtok.kind == TK_LE) { next_token(); n = new_bin(ND_LE, n, parse_add()); }
        else if (curtok.kind == TK_GE) { next_token(); n = new_bin(ND_GE, n, parse_add()); }
        else if (curtok.kind == TK_EQ) { next_token(); n = new_bin(ND_EQ, n, parse_add()); }
        else { next_token(); n = new_bin(ND_NE, n, parse_add()); }
    }
    return n;
}

Node *parse_and() {
    Node *n = parse_cmp();
    while (curtok.kind == TK_AND) {
        next_token();
        n = new_bin(ND_AND, n, parse_cmp());
    }
    return n;
}

Node *parse_or() {
    Node *n = parse_and();
    while (curtok.kind == TK_OR) {
        next_token();
        n = new_bin(ND_OR, n, parse_and());
    }
    return n;
}

Node *parse_assign() {
    Node *n = parse_or();
    if (curtok.kind == TK_ASSIGN) {
        next_token();
        Node *rhs = parse_assign();
        n = new_bin(ND_ASSIGN, n, rhs);
    }
    return n;
}

Node *parse_expr() { return parse_assign(); }

Node *parse_stmt() {
    if (curtok.kind == TK_KW_ASM) {
        next_token();
        expect(TK_LPAREN);
        if (curtok.kind != TK_STRING) { fprintf(stderr, "Expected string in asm()\n"); exit(1); }
        Node *asmn = new_stmt(ND_ASM);
        asmn->name = curtok.text;
        next_token();
        expect(TK_RPAREN);
        expect(TK_SEMI);
        return asmn;
    }
    if (curtok.kind == TK_KW_INT) {
        next_token();
        if (curtok.kind != TK_IDENT) { fprintf(stderr, "Expected identifier\n"); exit(1); }
        char *name = curtok.text;
        next_token();
        Node *init = NULL;
        if (curtok.kind == TK_ASSIGN) {
            next_token();
            init = parse_expr();
        }
        expect(TK_SEMI);
        scope_add_var(name);
        if (init) {
            Node *assign = new_bin(ND_ASSIGN, new_var(name), init);
            return assign;
        }
        return new_num(0);  /* dummy node */
    }
    if (curtok.kind == TK_KW_RETURN) {
        next_token();
        Node *ret = new_stmt(ND_RETURN);
        ret->lhs = parse_expr();
        expect(TK_SEMI);
        return ret;
    }
    if (curtok.kind == TK_KW_IF) {
        next_token();
        expect(TK_LPAREN);
        Node *cond = parse_expr();
        expect(TK_RPAREN);
        expect(TK_LBRACE);
        Node *body = parse_stmt();
        expect(TK_RBRACE);
        Node *ifn = new_stmt(ND_IF);
        ifn->cond = cond;
        ifn->body = body;
        if (curtok.kind == TK_KW_ELSE) {
            next_token();
            expect(TK_LBRACE);
            ifn->els = parse_stmt();
            expect(TK_RBRACE);
        }
        return ifn;
    }
    if (curtok.kind == TK_KW_WHILE) {
        next_token();
        expect(TK_LPAREN);
        Node *cond = parse_expr();
        expect(TK_RPAREN);
        expect(TK_LBRACE);
        Node *body = parse_stmt();
        expect(TK_RBRACE);
        Node *whilen = new_stmt(ND_WHILE);
        whilen->cond = cond;
        whilen->body = body;
        return whilen;
    }
    if (curtok.kind == TK_KW_FOR) {
        next_token();
        expect(TK_LPAREN);
        Node *init = (curtok.kind == TK_SEMI) ? NULL : parse_expr();
        expect(TK_SEMI);
        Node *cond = (curtok.kind == TK_SEMI) ? NULL : parse_expr();
        expect(TK_SEMI);
        Node *incr = (curtok.kind == TK_RPAREN) ? NULL : parse_expr();
        expect(TK_RPAREN);
        expect(TK_LBRACE);
        Node *body = parse_stmt();
        expect(TK_RBRACE);
        Node *forn = new_stmt(ND_FOR);
        forn->init = init;
        forn->cond = cond;
        forn->incr = incr;
        forn->body = body;
        return forn;
    }
    if (curtok.kind == TK_LBRACE) {
        next_token();
        Node *head = NULL, *tail = NULL;
        while (curtok.kind != TK_RBRACE) {
            Node *stmt = parse_stmt();
            if (!head) head = stmt;
            else if (tail) tail->next = stmt;
            tail = stmt;
        }
        expect(TK_RBRACE);
        return head ? head : new_num(0);
    }
    /* expression statement */
    Node *expr = parse_expr();
    expect(TK_SEMI);
    return expr;
}

/* Code generation */
void gen_expr(Node *n);

void gen_expr(Node *n) {
    if (!n) return;
    if (n->kind == ND_NUM) {
        emit("    mov r0, #%ld\n", n->val);
        return;
    }
    if (n->kind == ND_VAR) {
        int off = scope_get_var_offset(n->name);
        emit("    mov r0, r15\n");
        emit("    add r0, r0, #%d\n", off);
        emit("    ldr r0, r0, #0\n");
        return;
    }
    if (n->kind == ND_CALL) {
        /* Simple calling convention: arg in r0 if present */
        if (n->lhs) {
            gen_expr(n->lhs);
            /* r0 already has arg */
        }
        emit("    call $%s\n", n->name);
        return;
    }
    if (n->kind == ND_ASSIGN) {
        gen_expr(n->rhs);
        emit("    mov r1, r0\n");
        if (n->lhs->kind != ND_VAR) { fprintf(stderr, "Invalid assignment target\n"); exit(1); }
        int off = scope_get_var_offset(n->lhs->name);
        emit("    mov r2, r15\n");
        emit("    add r2, r2, #%d\n", off);
        emit("    str r1, r2, #0\n");
        emit("    mov r0, r1\n");
        return;
    }
    if (n->kind == ND_ADD || n->kind == ND_SUB || n->kind == ND_MUL || n->kind == ND_DIV || n->kind == ND_MOD ||
        n->kind == ND_LT || n->kind == ND_GT || n->kind == ND_LE || n->kind == ND_GE || n->kind == ND_EQ || n->kind == ND_NE) {
        gen_expr(n->lhs);
        emit("    mov r1, r0\n");
        gen_expr(n->rhs);
        emit("    mov r2, r0\n");
        emit("    mov r0, r1\n");
        if (n->kind == ND_ADD) emit("    add r0, r0, r2\n");
        else if (n->kind == ND_SUB) emit("    sub r0, r0, r2\n");
        else if (n->kind == ND_MUL) emit("    mul r0, r0, r2\n");
        else if (n->kind == ND_DIV) emit("    div r0, r0, r2\n");
        else if (n->kind == ND_MOD) emit("    mod r0, r0, r2\n");
        else if (n->kind == ND_LT) { emit("    cmp r0, r2\n"); emit("    jl $.true\n"); emit("    mov r0, #0\n"); emit("    jmp $.false\n"); emit(".true:\n"); emit("    mov r0, #1\n"); emit(".false:\n"); }
        else if (n->kind == ND_GT) { emit("    cmp r0, r2\n"); emit("    jg $.true\n"); emit("    mov r0, #0\n"); emit("    jmp $.false\n"); emit(".true:\n"); emit("    mov r0, #1\n"); emit(".false:\n"); }
        else if (n->kind == ND_LE) { emit("    cmp r0, r2\n"); emit("    jle $.true\n"); emit("    mov r0, #0\n"); emit("    jmp $.false\n"); emit(".true:\n"); emit("    mov r0, #1\n"); emit(".false:\n"); }
        else if (n->kind == ND_GE) { emit("    cmp r0, r2\n"); emit("    jge $.true\n"); emit("    mov r0, #0\n"); emit("    jmp $.false\n"); emit(".true:\n"); emit("    mov r0, #1\n"); emit(".false:\n"); }
        else if (n->kind == ND_EQ) { emit("    cmp r0, r2\n"); emit("    je $.true\n"); emit("    mov r0, #0\n"); emit("    jmp $.false\n"); emit(".true:\n"); emit("    mov r0, #1\n"); emit(".false:\n"); }
        else if (n->kind == ND_NE) { emit("    cmp r0, r2\n"); emit("    jne $.true\n"); emit("    mov r0, #0\n"); emit("    jmp $.false\n"); emit(".true:\n"); emit("    mov r0, #1\n"); emit(".false:\n"); }
        return;
    }
}

// void gen_stmt(Node *n);

void gen_stmt(Node *n) {
    if (!n) return;
    if (n->kind == ND_ASM) {
        emit("%s\n", n->name);
        if (n->next) gen_stmt(n->next);
        return;
    }
    if (n->kind == ND_RETURN) {
        gen_expr(n->lhs);
        emit("    ret\n");
        if (n->next) gen_stmt(n->next);
        return;
    }
    if (n->kind == ND_IF) {
        gen_expr(n->cond);
        char *else_label = gen_label();
        char *done_label = gen_label();
        emit("    cmp r0, #0\n");
        emit("    je $%s\n", else_label);
        gen_stmt(n->body);
        emit("    jmp $%s\n", done_label);
        emit("%s:\n", else_label);
        if (n->els) gen_stmt(n->els);
        emit("%s:\n", done_label);
        if (n->next) gen_stmt(n->next);
        return;
    }
    if (n->kind == ND_WHILE) {
        char *loop_label = gen_label();
        char *done_label = gen_label();
        emit("%s:\n", loop_label);
        gen_expr(n->cond);
        emit("    cmp r0, #0\n");
        emit("    je $%s\n", done_label);
        gen_stmt(n->body);
        emit("    jmp $%s\n", loop_label);
        emit("%s:\n", done_label);
        if (n->next) gen_stmt(n->next);
        return;
    }
    if (n->kind == ND_FOR) {
        if (n->init) gen_expr(n->init);
        char *loop_label = gen_label();
        char *done_label = gen_label();
        emit("%s:\n", loop_label);
        if (n->cond) {
            gen_expr(n->cond);
            emit("    cmp r0, #0\n");
            emit("    je $%s\n", done_label);
        }
        gen_stmt(n->body);
        if (n->incr) gen_expr(n->incr);
        emit("    jmp $%s\n", loop_label);
        emit("%s:\n", done_label);
        if (n->next) gen_stmt(n->next);
        return;
    }
    /* Statement list */
    if (n->next) {
        gen_stmt(n);
        gen_stmt(n->next);
        return;
    }
    gen_expr(n);
}

void compile_function(const char *name, int arg_count, char **arg_names) {
    emit("%s:\n", name);
    scope_enter(arg_count);
    
    /* Add parameters to scope */
    for (int i = 0; i < arg_count; i++) {
        scope_add_var(arg_names[i]);
    }
    
    /* Parse function body */
    expect(TK_LBRACE);
    Node *head = NULL, *tail = NULL;
    while (curtok.kind != TK_RBRACE) {
        Node *stmt = parse_stmt();
        if (!head) head = stmt;
        else if (tail) tail->next = stmt;
        tail = stmt;
    }
    expect(TK_RBRACE);
    
    /* Generate code */
    gen_stmt(head);
    
    scope_exit();
}

void emit_start() {
    emit("_start:\n"
        "    call $main\n"
        "    hlt\n"
    );
}

void compile(FILE *out) {
    out_file = out;
    emit_start();
    while (curtok.kind != TK_EOF) {
        if (curtok.kind == TK_KW_INT) {
            next_token();
            if (curtok.kind != TK_IDENT) { fprintf(stderr, "Expected function name\n"); exit(1); }
            char *fname = strdup(curtok.text);
            functions[function_counter++] = fname;
            next_token();
            expect(TK_LPAREN);
            char **arg_names = malloc(sizeof(char*) * 4);
            int arg_count = 0;
            if (curtok.kind != TK_RPAREN) {
                /* Parse parameters */
                expect(TK_KW_INT);
                if (curtok.kind == TK_IDENT) {
                    arg_names[arg_count] = strdup(curtok.text);
                    next_token();
                    arg_count = 1;
                }
                while (curtok.kind == TK_COMMA) {
                    next_token();
                    expect(TK_KW_INT);
                    if (curtok.kind == TK_IDENT) {
                        arg_names[arg_count] = strdup(curtok.text);
                        next_token();
                        arg_count++;
                    }
                }
            }
            expect(TK_RPAREN);
            compile_function(fname, arg_count, arg_names);
            emit("\n");
        } else {
            fprintf(stderr, "Unexpected token at top level\n"); exit(1);
        }
    }
    
    int found_main = 0;
    for (int i = 0; i < function_counter; i++) {
        if (strcmp(functions[i], "main") == 0) {
            found_main = 1; break;
        }
    }
    if (found_main == 0) {
        fprintf(stderr, "Missing main function\n"); exit(1);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <input.c> [output.s]\n", argv[0]); return 1; }
    const char *infile = argv[1];
    const char *outfile = (argc >= 3) ? argv[2] : "-";

    FILE *f = fopen(infile, "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END); size_t sz = ftell(f); fseek(f,0,SEEK_SET);
    src = malloc(sz+1); fread((void*)src,1,sz,f); ((char*)src)[sz]='\0'; fclose(f);
    pos = 0;
    next_token();

    functions = malloc(sizeof(char*) * 1024);

    FILE *out = stdout;
    if (outfile[0] != '-' ) {
        out = fopen(outfile, "wb");
        if (!out) { perror("fopen out"); return 1; }
    }

    compile(out);

    if (out != stdout) fclose(out);
    return 0;
}
