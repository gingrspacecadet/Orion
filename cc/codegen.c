/* codegen.c
   Emits custom ISA assembly using registers r0..r15 and mnemonics
   matching your instruction set and textual conventions.
   PUSH/POP use register-bitmask immediates (e.g. #0x000F).
*/

#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

/* Small symbol table per function mapping local var name -> stack slot (word index) */
typedef struct Var {
    char *name;
    int slot;              /* word offset from frame base (words) */
    struct Var *next;
} Var;

typedef struct {
    Var *vars;
    int next_slot;         /* next available local slot (words) */
    FILE *out;
    int label_id;
    int break_label;       /* label to jump to on break */
    int continue_label;    /* label to jump to on continue */
} CGContext;

static int newlabel(CGContext *c) { return ++c->label_id; }
static void cg_error(const char *s) { fprintf(stderr, "codegen error: %s\n", s); exit(1); }

static void add_local(CGContext *c, const char *name) {
    Var *v = malloc(sizeof(Var));
    v->name = strdup(name);
    v->slot = ++c->next_slot; /* first local is slot 1, etc. */
    v->next = c->vars;
    c->vars = v;
}

static int find_local(CGContext *c, const char *name) {
    Var *v = c->vars;
    while (v) {
        if (strcmp(v->name, name) == 0) return v->slot;
        v = v->next;
    }
    return -1;
}

/* Utility: print register name from index (0..15) */
static const char *rname(int idx) {
    static char buf[8];
    if (idx < 0 || idx > 15) sprintf(buf, "r%d", idx);
    else sprintf(buf, "r%d", idx);
    return buf;
}

/* helper to emit comment */
static void emitc(CGContext *c, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(c->out, "    ; ");
    vfprintf(c->out, fmt, ap);
    fprintf(c->out, "\n");
    va_end(ap);
}

/* We'll use r12 as FP (frame pointer) and r13 as SP (stack pointer).
   Frame layout (word addressed):
     FP (r12) points to saved FP at offset 0, saved LR at offset 1, locals start at slot 1 which maps to imm = slot+1
*/

/* forward declarations */
static void cg_expr(CGContext *c, ASTNode *n);
static void cg_stmt(CGContext *c, ASTNode *n);

/* emit prologue for function fnname */
static void emit_prologue(CGContext *c, const char *fnname) {
    fprintf(c->out, "%s:\n", fnname);
    /* push FP and LR onto stack: PUSH with register bitmask (bits 12 and 15) */
    unsigned int mask = (1u<<12) | (1u<<15);
    fprintf(c->out, "    PUSH #0x%04x    ; save FP(r12) and LR(r15)\n", (unsigned int)mask);
    /* set FP = SP (after push) -> MOV r12, r13 */
    fprintf(c->out, "    MOV r12, r13\n");
}

/* emit epilogue */
static void emit_epilogue(CGContext *c) {
    // /* default return: move 0 into r0 */
    // fprintf(c->out, "    MOV r0, 0    ; default return 0\n");
    // /* restore FP and LR via POP of same mask */
    // unsigned int mask = (1u<<12) | (1u<<15);
    // fprintf(c->out, "    POP r0, r0, 0x%04x    ; restore FP(r12) and LR(r15)\n", (unsigned int)mask);
    // /* return: jump to r15 (link register) */
    // fprintf(c->out, "    RET\n");
    // fprintf(c->out, "; === end function ===\n\n");
}

/* Access local variable slot: generate LDR/STR using base = r12 (FP) and imm16 offset (words) */
static void emit_load_local(CGContext *c, int slot, const char *dst_reg) {
    int imm = slot + 1; /* slot 1 -> imm 2 (skip saved FP and LR) */
    fprintf(c->out, "    LDR %s, r12, #%d    ; load local slot %d\n", dst_reg, imm, slot);
}

static void emit_store_local(CGContext *c, int slot, const char *src_reg) {
    int imm = slot + 1;
    fprintf(c->out, "    STR %s, r12, #%d    ; store local slot %d\n", src_reg, imm, slot);
}

/* Evaluate expression and place result in r0 (return register) */
static void cg_expr(CGContext *c, ASTNode *n) {
    if (!n) return;
    switch (n->type) {
    case NODE_INT:
        /* MOV r0, #imm */
        fprintf(c->out, "    MOV r0, #%d\n", n->u.ival);
        break;
    case NODE_VAR: {
        int slot = find_local(c, n->u.name);
        if (slot == -1) { cg_error("unknown variable"); }
        emit_load_local(c, slot, "r0");
        break;
    }
    case NODE_STRING:
        /* For now, just emit a warning (string handling requires data section) */
        fprintf(c->out, "    ; TODO: string literal: %s\n", n->u.str);
        fprintf(c->out, "    MOV r0, #0\n");
        break;
    case NODE_UNARY: {
        cg_expr(c, n->u.unary.expr);
        switch (n->u.unary.op) {
        case OP_NEG:
            fprintf(c->out, "    SUB r0, #0, r0    ; r0 = 0 - r0\n");
            break;
        case OP_NOT:
            {
                int Ltrue = newlabel(c);
                int Ldone = newlabel(c);
                fprintf(c->out, "    CMP r0, #0\n");
                fprintf(c->out, "    JE $L%04d\n", Ltrue);
                fprintf(c->out, "    MOV r0, #0\n");
                fprintf(c->out, "    JMP $L%04d\n", Ldone);
                fprintf(c->out, "L%04d:\n", Ltrue);
                fprintf(c->out, "    MOV r0, #1\n");
                fprintf(c->out, "L%04d:\n", Ldone);
            }
            break;
        case OP_BITWISE_XOR:
            fprintf(c->out, "    XOR r0, r0, #0xFFFFFFFF    ; bitwise NOT\n");
            break;
        default:
            cg_error("unsupported unary op");
        }
        break;
    }
    case NODE_ASSIGN: {
        /* evaluate rhs into r0, then store into local */
        cg_expr(c, n->u.assign.rhs);
        int slot = find_local(c, n->u.assign.name);
        if (slot == -1) { add_local(c, n->u.assign.name); slot = find_local(c, n->u.assign.name); }
        emit_store_local(c, slot, "r0");
        break;
    }
    case NODE_ARRAY_ACCESS: {
        /* For now, emit basic array access (simplified) */
        cg_expr(c, n->u.array_access.index);
        fprintf(c->out, "    ; TODO: array access: %s[...]\n", n->u.array_access.name);
        break;
    }
    case NODE_BINOP: {
        /* Evaluate left into r1, right into r0, then perform op producing r0 */
        cg_expr(c, n->u.binop.l);           /* r0 = left */
        fprintf(c->out, "    MOV r1, r0\n"); /* r1 = left */
        cg_expr(c, n->u.binop.r);           /* r0 = right */

        /* perform operation with r1 (left) and r0 (right), result in r0 */
        switch (n->u.binop.op) {
        case OP_ADD:
            fprintf(c->out, "    ADD r0, r1, r0    ; r0 = r1 + r0\n");
            break;
        case OP_SUB:
            fprintf(c->out, "    SUB r0, r1, r0    ; r0 = r1 - r0\n");
            break;
        case OP_MUL:
            fprintf(c->out, "    MUL r0, r1, r0    ; r0 = r1 * r0\n");
            break;
        case OP_DIV:
            fprintf(c->out, "    DIV r0, r1, r0    ; r0 = r1 / r0\n");
            break;
        case OP_MOD:
            fprintf(c->out, "    MOD r0, r1, r0    ; r0 = r1 %% r0\n");
            break;
        case OP_BITWISE_AND:
            fprintf(c->out, "    AND r0, r1, r0    ; r0 = r1 & r0\n");
            break;
        case OP_BITWISE_OR:
            fprintf(c->out, "    OR r0, r1, r0    ; r0 = r1 | r0\n");
            break;
        case OP_BITWISE_XOR:
            fprintf(c->out, "    XOR r0, r1, r0    ; r0 = r1 ^ r0\n");
            break;
        case OP_LSHIFT:
            fprintf(c->out, "    SHL r0, r1, r0    ; r0 = r1 << r0\n");
            break;
        case OP_RSHIFT:
            fprintf(c->out, "    SHR r0, r1, r0    ; r0 = r1 >> r0\n");
            break;
        case OP_EQ:
            /* r0 = (r1 == r0) ? 1 : 0 */
            {
                int Ltrue = newlabel(c);
                int Ldone = newlabel(c);
                fprintf(c->out, "    CMP r1, r0\n");
                fprintf(c->out, "    JE $L%04d\n", Ltrue);
                fprintf(c->out, "    MOV r0, #0\n");
                fprintf(c->out, "    JMP $L%04d\n", Ldone);
                fprintf(c->out, "L%04d:\n", Ltrue);
                fprintf(c->out, "    MOV r0, #1\n");
                fprintf(c->out, "L%04d:\n", Ldone);
            }
            break;
        case OP_NEQ:
            {
                int Ltrue = newlabel(c);
                int Ldone = newlabel(c);
                fprintf(c->out, "    CMP r1, r0\n");
                fprintf(c->out, "    JNE $L%04d\n", Ltrue);
                fprintf(c->out, "    MOV r0, #0\n");
                fprintf(c->out, "    JMP $L%04d\n", Ldone);
                fprintf(c->out, "L%04d:\n", Ltrue);
                fprintf(c->out, "    MOV r0, #1\n");
                fprintf(c->out, "L%04d:\n", Ldone);
            }
            break;
        case OP_LT:
            {
                int Ltrue = newlabel(c);
                int Ldone = newlabel(c);
                fprintf(c->out, "    CMP r1, r0\n");
                fprintf(c->out, "    JIL $L%04d\n", Ltrue);
                fprintf(c->out, "    MOV r0, #0\n");
                fprintf(c->out, "    JMP $L%04d\n", Ldone);
                fprintf(c->out, "L%04d:\n", Ltrue);
                fprintf(c->out, "    MOV r0, #1\n");
                fprintf(c->out, "L%04d:\n", Ldone);
            }
            break;
        case OP_GT:
            {
                int Ltrue = newlabel(c);
                int Ldone = newlabel(c);
                fprintf(c->out, "    CMP r1, r0\n");
                fprintf(c->out, "    JG $L%04d\n", Ltrue);
                fprintf(c->out, "    MOV r0, #0\n");
                fprintf(c->out, "    JMP $L%04d\n", Ldone);
                fprintf(c->out, "L%04d:\n", Ltrue);
                fprintf(c->out, "    MOV r0, #1\n");
                fprintf(c->out, "L%04d:\n", Ldone);
            }
            break;
        case OP_LE:
            {
                int Ltrue = newlabel(c);
                int Ldone = newlabel(c);
                fprintf(c->out, "    CMP r1, r0\n");
                fprintf(c->out, "    JLE $L%04d\n", Ltrue);
                fprintf(c->out, "    MOV r0, #0\n");
                fprintf(c->out, "    JMP $L%04d\n", Ldone);
                fprintf(c->out, "L%04d:\n", Ltrue);
                fprintf(c->out, "    MOV r0, #1\n");
                fprintf(c->out, "L%04d:\n", Ldone);
            }
            break;
        case OP_GE:
            {
                int Ltrue = newlabel(c);
                int Ldone = newlabel(c);
                fprintf(c->out, "    CMP r1, r0\n");
                fprintf(c->out, "    JGE $L%04d\n", Ltrue);
                fprintf(c->out, "    MOV r0, #0\n");
                fprintf(c->out, "    JMP $L%04d\n", Ldone);
                fprintf(c->out, "L%04d:\n", Ltrue);
                fprintf(c->out, "    MOV r0, #1\n");
                fprintf(c->out, "L%04d:\n", Ldone);
            }
            break;
        case OP_AND:
            fprintf(c->out, "    AND r0, r1, r0\n");
            break;
        case OP_OR:
            fprintf(c->out, "    OR r0, r1, r0\n");
            break;
        default:
            cg_error("unsupported binary op");
        }
        break;
    }
    case NODE_CALL: {
        /* Evaluate up to 4 args into r0..r3 (left-to-right), then CALL $label */
        ASTNodeList *a = n->u.call.args;
        int idx = 0;
        while (a && idx < 4) {
            cg_expr(c, a->node);
            /* move from r0 to r{idx} */
            fprintf(c->out, "    MOV %s, r0    ; arg %d\n", rname(idx), idx);
            idx++; a = a->next;
        }
        /* Call label via $label notation */
        fprintf(c->out, "    CALL $%s\n", n->u.call.fname);
        /* return value in r0 */
        break;
    }
    default:
        cg_error("unhandled expr node in cg_expr");
    }
}

/* Generate code for statements */
static void cg_stmt(CGContext *c, ASTNode *n) {
    if (!n) return;
    switch (n->type) {
    case NODE_RETURN:
        cg_expr(c, n->u.ret.expr);
        /* restore FP and LR and return; use POP mask for bits 12 and 15 */
        {
            unsigned int mask = (1u<<12) | (1u<<15);
            fprintf(c->out, "    POP #0x%04x    ; restore FP and LR\n", mask);
        }
        fprintf(c->out, "    RET\n");
        break;
    case NODE_BREAK:
        if (c->break_label == 0) cg_error("break outside loop");
        fprintf(c->out, "    JMP $L%04d    ; break\n", c->break_label);
        break;
    case NODE_CONTINUE:
        if (c->continue_label == 0) cg_error("continue outside loop");
        fprintf(c->out, "    JMP $L%04d    ; continue\n", c->continue_label);
        break;
    case NODE_ASSIGN:
        cg_expr(c, n->u.assign.rhs);
        {
            int slot = find_local(c, n->u.assign.name);
            if (slot == -1) { add_local(c, n->u.assign.name); slot = find_local(c, n->u.assign.name); }
            emit_store_local(c, slot, "r0");
        }
        break;
    case NODE_IF: {
        int Lelse = newlabel(c);
        int Lend = newlabel(c);
        cg_expr(c, n->u.ifelse.cond);    /* r0 = cond */
        fprintf(c->out, "    CMP r0, #0\n");
        fprintf(c->out, "    JE $L%04d\n", Lelse);
        cg_stmt(c, n->u.ifelse.thenb);
        fprintf(c->out, "    JMP $L%04d\n", Lend);
        fprintf(c->out, "L%04d:\n", Lelse);
        if (n->u.ifelse.elseb) cg_stmt(c, n->u.ifelse.elseb);
        fprintf(c->out, "L%04d:\n", Lend);
        break;
    }
    case NODE_WHILE: {
        int Ltop = newlabel(c);
        int Lend = newlabel(c);
        int prev_break = c->break_label;
        int prev_continue = c->continue_label;
        c->break_label = Lend;
        c->continue_label = Ltop;
        fprintf(c->out, "L%04d:\n", Ltop);
        cg_expr(c, n->u.while_node.cond);
        fprintf(c->out, "    CMP r0, #0\n");
        fprintf(c->out, "    JE $L%04d\n", Lend);
        cg_stmt(c, n->u.while_node.body);
        fprintf(c->out, "    JMP $L%04d\n", Ltop);
        fprintf(c->out, "L%04d:\n", Lend);
        c->break_label = prev_break;
        c->continue_label = prev_continue;
        break;
    }
    case NODE_DO_WHILE: {
        int Ltop = newlabel(c);
        int Lend = newlabel(c);
        int prev_break = c->break_label;
        int prev_continue = c->continue_label;
        c->break_label = Lend;
        c->continue_label = Ltop;
        fprintf(c->out, "L%04d:\n", Ltop);
        cg_stmt(c, n->u.do_while_node.body);
        cg_expr(c, n->u.do_while_node.cond);
        fprintf(c->out, "    CMP r0, #0\n");
        fprintf(c->out, "    JNE $L%04d\n", Ltop);
        fprintf(c->out, "L%04d:\n", Lend);
        c->break_label = prev_break;
        c->continue_label = prev_continue;
        break;
    }
    case NODE_FOR: {
        int Ltop = newlabel(c);
        int Lupdate = newlabel(c);
        int Lend = newlabel(c);
        int prev_break = c->break_label;
        int prev_continue = c->continue_label;
        c->break_label = Lend;
        c->continue_label = Lupdate;
        /* init expression */
        if (n->u.for_node.init) cg_expr(c, n->u.for_node.init);
        fprintf(c->out, "L%04d:\n", Ltop);
        /* condition */
        if (n->u.for_node.cond) {
            cg_expr(c, n->u.for_node.cond);
            fprintf(c->out, "    CMP r0, #0\n");
            fprintf(c->out, "    JE $L%04d\n", Lend);
        }
        /* body */
        cg_stmt(c, n->u.for_node.body);
        /* update */
        fprintf(c->out, "L%04d:\n", Lupdate);
        if (n->u.for_node.update) cg_expr(c, n->u.for_node.update);
        fprintf(c->out, "    JMP $L%04d\n", Ltop);
        fprintf(c->out, "L%04d:\n", Lend);
        c->break_label = prev_break;
        c->continue_label = prev_continue;
        break;
    }
    case NODE_BLOCK: {
        /* Add local declarations */
        ASTNodeList *d = n->u.block.decls;
        while (d) {
            if (d->node->type == NODE_VAR) add_local(c, d->node->u.name);
            d = d->next;
        }
        ASTNodeList *s = n->u.block.stmts;
        while (s) {
            cg_stmt(c, s->node);
            s = s->next;
        }
        break;
    }
    case NODE_CALL:
        cg_expr(c, n); /* emit call (call leaves return in r0) */
        break;
    case NODE_BINOP:
    case NODE_INT:
    case NODE_VAR:
        /* expression statement: evaluate and discard result */
        cg_expr(c, n);
        break;
    default:
        cg_error("unhandled stmt node in cg_stmt");
    }
}

/* Top-level codegen iteration over functions */
void codegen_all(FILE *out) {
    CGContext ctx = {0};
    ctx.out = out;
    ASTNodeList *f = ast_get_functions();

    {
        bool user_start = false;
        ASTNodeList *tmp = ast_get_functions();
        while (tmp) {
            if (tmp->node->type == NODE_FUNC && strcmp(tmp->node->u.func.fname, "_start") == 0) {
                user_start = true;
                break;
            }
            tmp = tmp->next;
        }

        /* if user did not provide _start, emit a default one */
        if (!user_start) {
            fprintf(out, "_start:\n");
            /* set SP and FP: choose a constant or expose macro/define */
            fprintf(out, "    MOV r13, #0xFFFF    ; init SP (word address)\n");
            fprintf(out, "    SHL r13, r13, #16\n");
            fprintf(out, "    OR  r13, r13, #0xFFFF\n");
            fprintf(out, "    MOV r12, r13    ; init FP\n");
            /* zero argc/argv to be safe */
            fprintf(out, "    MOV r0, #0\n");
            fprintf(out, "    MOV r1, #0\n");
            /* call main */
            fprintf(out, "    CALL $main\n");
            /* move return value and either INT or HLT */
            fprintf(out, "    MOV r1, r0\n");
            fprintf(out, "    INT #0\n");
            fprintf(out, "    HLT\n\n");
        }
    }

    while (f) {
        ASTNode *fn = f->node;
        if (fn->type != NODE_FUNC) { f = f->next; continue; }
        /* reset context for each function */
        ctx.vars = NULL;
        ctx.next_slot = 0; /* start slots at 1 when first added */
        ctx.label_id = 0;

        emit_prologue(&ctx, fn->u.func.fname);

        /* move parameters into locals (params r0..r3) */
        ASTNodeList *p = fn->u.func.params;
        int pidx = 0;
        while (p) {
            if (pidx < 4) {
                add_local(&ctx, p->node->u.name);
                int slot = find_local(&ctx, p->node->u.name);
                /* store r{pidx} into local slot */
                fprintf(out, "    STR %s, r12, #%d    ; param %d -> local slot %d\n", rname(pidx), (slot+1), pidx, slot);
            } else {
                fprintf(out, "    ; warning: param %d not moved to local (only first 4 params supported)\n", pidx);
            }
            pidx++; p = p->next;
        }

        /* generate body */
        cg_stmt(&ctx, fn->u.func.body);

        /* epilogue */
        emit_epilogue(&ctx);

        f = f->next;
    }
}
