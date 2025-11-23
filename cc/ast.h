#ifndef AST_H
#define AST_H

#include <stddef.h>

typedef enum {
    NODE_INT,
    NODE_VAR,
    NODE_ASSIGN,
    NODE_BINOP,
    NODE_RETURN,
    NODE_IF,
    NODE_WHILE,
    NODE_BLOCK,
    NODE_EXPR_STMT,
    NODE_CALL,
    NODE_FUNC
} NodeType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_AND, OP_OR
} BinOp;

struct ASTNode;
struct ASTNodeList;

typedef struct ASTNode {
    NodeType type;
    int line;
    union {
        int ival;                /* int const */
        char *name;              /* var or function name */
        struct { char *name; struct ASTNode *rhs; } assign;
        struct { BinOp op; struct ASTNode *l, *r; } binop;
        struct { struct ASTNode *cond; struct ASTNode *thenb; struct ASTNode *elseb; } ifelse;
        struct { struct ASTNode *cond; struct ASTNode *body; } while_node;
        struct { struct ASTNodeList *decls; struct ASTNodeList *stmts;} block;
        struct { struct ASTNode *expr; } ret;
        struct { char *fname; struct ASTNodeList *args; } call;
        struct { char *fname; struct ASTNodeList *params; struct ASTNode *body; } func;
    } u;
} ASTNode;

typedef struct ASTNodeList {
    ASTNode *node;
    struct ASTNodeList *next;
} ASTNodeList;

/* builder functions */
ASTNode *ast_int_new(int v);
ASTNode *ast_var_new(char *name);
ASTNode *ast_assign_new(char *name, ASTNode *rhs);
ASTNode *ast_binop_new(BinOp op, ASTNode *l, ASTNode *r);
ASTNode *ast_return_new(ASTNode *e);
ASTNode *ast_if_new(ASTNode *cond, ASTNode *thenb, ASTNode *elseb);
ASTNode *ast_while_new(ASTNode *cond, ASTNode *body);
ASTNode *ast_block_new(ASTNodeList *decls, ASTNodeList *stmts);
ASTNode *ast_call_new(char *name, ASTNodeList *args);
ASTNode *ast_function_new(char *name, ASTNodeList *params, ASTNode *body);

ASTNodeList *ast_stmt_list_new(ASTNode *n);
ASTNodeList *ast_stmt_list_add(ASTNodeList *list, ASTNode *n);
ASTNodeList *ast_param_list_new(char *name);
ASTNodeList *ast_param_list_add(ASTNodeList *list, char *name);
ASTNodeList *ast_arg_list_new(ASTNode *n);
ASTNodeList *ast_arg_list_add(ASTNodeList *list, ASTNode *n);
ASTNodeList *ast_decl_add(ASTNodeList *list, char *name);

void ast_add_function(ASTNode *f);
ASTNodeList *ast_get_functions(void);

/* memory cleanup omitted for brevity */

#endif
