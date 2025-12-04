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
    NODE_FOR,
    NODE_DO_WHILE,
    NODE_BLOCK,
    NODE_EXPR_STMT,
    NODE_CALL,
    NODE_FUNC,
    NODE_UNARY,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_ARRAY_ACCESS,
    NODE_ARRAY_DECL,
    NODE_STRING
} NodeType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_AND, OP_OR,
    OP_NEG, OP_NOT, OP_BITWISE_AND, OP_BITWISE_OR, OP_BITWISE_XOR,
    OP_LSHIFT, OP_RSHIFT
} BinOp;

struct ASTNode;
struct ASTNodeList;

typedef struct ASTNode {
    NodeType type;
    int line;
    union {
        int ival;                /* int const */
        char *name;              /* var or function name */
        char *str;               /* string literal */
        struct { char *name; struct ASTNode *rhs; } assign;
        struct { BinOp op; struct ASTNode *l, *r; } binop;
        struct { BinOp op; struct ASTNode *expr; } unary;
        struct { struct ASTNode *cond; struct ASTNode *thenb; struct ASTNode *elseb; } ifelse;
        struct { struct ASTNode *cond; struct ASTNode *body; } while_node;
        struct { struct ASTNode *init; struct ASTNode *cond; struct ASTNode *update; struct ASTNode *body; } for_node;
        struct { struct ASTNode *body; struct ASTNode *cond; } do_while_node;
        struct { struct ASTNodeList *decls; struct ASTNodeList *stmts;} block;
        struct { struct ASTNode *expr; } ret;
        struct { char *fname; struct ASTNodeList *args; } call;
        struct { char *fname; struct ASTNodeList *params; struct ASTNode *body; } func;
        struct { char *name; struct ASTNode *index; } array_access;
        struct { char *name; int size; } array_decl;
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
ASTNode *ast_unary_new(BinOp op, ASTNode *expr);
ASTNode *ast_return_new(ASTNode *e);
ASTNode *ast_if_new(ASTNode *cond, ASTNode *thenb, ASTNode *elseb);
ASTNode *ast_while_new(ASTNode *cond, ASTNode *body);
ASTNode *ast_for_new(ASTNode *init, ASTNode *cond, ASTNode *update, ASTNode *body);
ASTNode *ast_do_while_new(ASTNode *body, ASTNode *cond);
ASTNode *ast_block_new(ASTNodeList *decls, ASTNodeList *stmts);
ASTNode *ast_call_new(char *name, ASTNodeList *args);
ASTNode *ast_function_new(char *name, ASTNodeList *params, ASTNode *body);
ASTNode *ast_break_new(void);
ASTNode *ast_continue_new(void);
ASTNode *ast_array_access_new(char *name, ASTNode *index);
ASTNode *ast_array_decl_new(char *name, int size);
ASTNode *ast_string_new(char *str);

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
