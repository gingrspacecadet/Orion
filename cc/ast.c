#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static ASTNodeList *functions = NULL;

/* helpers */
static ASTNode *node_new(NodeType t) {
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type = t;
    return n;
}

ASTNode *ast_int_new(int v) {
    ASTNode *n = node_new(NODE_INT);
    n->u.ival = v;
    return n;
}
ASTNode *ast_var_new(char *name) {
    ASTNode *n = node_new(NODE_VAR);
    n->u.name = name;
    return n;
}
ASTNode *ast_assign_new(char *name, ASTNode *rhs) {
    ASTNode *n = node_new(NODE_ASSIGN);
    n->u.assign.name = name;
    n->u.assign.rhs = rhs;
    return n;
}
ASTNode *ast_binop_new(BinOp op, ASTNode *l, ASTNode *r) {
    ASTNode *n = node_new(NODE_BINOP);
    n->u.binop.op = op; n->u.binop.l = l; n->u.binop.r = r;
    return n;
}
ASTNode *ast_return_new(ASTNode *e) {
    ASTNode *n = node_new(NODE_RETURN);
    n->u.ret.expr = e; return n;
}
ASTNode *ast_if_new(ASTNode *cond, ASTNode *thenb, ASTNode *elseb) {
    ASTNode *n = node_new(NODE_IF);
    n->u.ifelse.cond = cond; n->u.ifelse.thenb = thenb; n->u.ifelse.elseb = elseb;
    return n;
}
ASTNode *ast_while_new(ASTNode *cond, ASTNode *body) {
    ASTNode *n = node_new(NODE_WHILE);
    n->u.while_node.cond = cond; n->u.while_node.body = body;
    return n;
}
ASTNode *ast_block_new(ASTNodeList *decls, ASTNodeList *stmts) {
    ASTNode *n = node_new(NODE_BLOCK);
    n->u.block.decls = decls; n->u.block.stmts = stmts;
    return n;
}
ASTNode *ast_call_new(char *name, ASTNodeList *args) {
    ASTNode *n = node_new(NODE_CALL);
    n->u.call.fname = name; n->u.call.args = args;
    return n;
}
ASTNode *ast_function_new(char *name, ASTNodeList *params, ASTNode *body) {
    ASTNode *n = node_new(NODE_FUNC);
    n->u.func.fname = name; n->u.func.params = params; n->u.func.body = body;
    return n;
}

/* lists */
ASTNodeList *ast_stmt_list_new(ASTNode *n) {
    ASTNodeList *l = calloc(1, sizeof(ASTNodeList));
    l->node = n;
    return l;
}
ASTNodeList *ast_stmt_list_add(ASTNodeList *list, ASTNode *n) {
    ASTNodeList *p = list;
    if (!p) return ast_stmt_list_new(n);
    while (p->next) p = p->next;
    p->next = ast_stmt_list_new(n);
    return list;
}
ASTNodeList *ast_param_list_new(char *name) { return ast_stmt_list_new(ast_var_new(name)); }
ASTNodeList *ast_param_list_add(ASTNodeList *list, char *name) { return ast_stmt_list_add(list, ast_var_new(name)); }
ASTNodeList *ast_arg_list_new(ASTNode *n) { return ast_stmt_list_new(n); }
ASTNodeList *ast_arg_list_add(ASTNodeList *list, ASTNode *n) { return ast_stmt_list_add(list, n); }
ASTNodeList *ast_decl_add(ASTNodeList *list, char *name) { return ast_stmt_list_add(list, ast_var_new(name)); }

/* function registry */
void ast_add_function(ASTNode *f) {
    ASTNodeList *l = calloc(1, sizeof(ASTNodeList));
    l->node = f; l->next = NULL;
    if (!functions) { functions = l; return; }
    ASTNodeList *p = functions; while (p->next) p = p->next; p->next = l;
}
ASTNodeList *ast_get_functions(void) { return functions; }
