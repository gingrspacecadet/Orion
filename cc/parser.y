%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

extern int yylex(void);
extern int yylineno;
void yyerror(const char *s);
%}

/* semantic value types */
%union {
    int ival;
    char *string;
    struct ASTNode *node;
    struct ASTNodeList *nlist;
}

/* tokens with types */
%token <string> IDENT STRING
%token <ival> INTEGER
%token INT RETURN IF ELSE WHILE FOR DO BREAK CONTINUE
%token EQ NEQ LE GE AND OR LSHIFT RSHIFT

/* Operator precedence and associativity */
%left OR
%left AND
%left '|'
%left '^'
%left '&'
%left EQ NEQ
%left '<' '>' LE GE
%left LSHIFT RSHIFT
%left '+' '-'
%left '*' '/' '%'
%right '!' '~' UNARY_MINUS

/* nonterminals with types */
%type <node> translation_unit function_def compound_stmt stmt expr
%type <nlist> decl_list stmt_list param_list arg_list

%%

translation_unit:
    /* empty */                     { $$ = NULL; }
  | translation_unit function_def   { ast_add_function($2); }
  ;

function_def:
    INT IDENT '(' ')' compound_stmt   {
        $$ = ast_function_new($2, NULL, $5);
    }
  | INT IDENT '(' param_list ')' compound_stmt {
        $$ = ast_function_new($2, $4, $6);
    }
  ;


param_list:
    INT IDENT                     { $$ = ast_param_list_new($2); }
  | param_list ',' INT IDENT     { $$ = ast_param_list_add($1, $4); }
  ;

compound_stmt:
    '{' decl_list stmt_list '}'    {
        $$ = ast_block_new($2, $3);
    }
  | '{' decl_list '}'              {
        $$ = ast_block_new($2, NULL);
    }
  | '{' stmt_list '}'              {
        $$ = ast_block_new(NULL, $2);
    }
  | '{' '}'                        {
        $$ = ast_block_new(NULL, NULL);
    }
  ;

decl_list:
    /* empty */                   { $$ = NULL; }
  | decl_list INT IDENT ';'      { $$ = ast_decl_add($1, $3); }
  ;

stmt_list:
    stmt                         { $$ = ast_stmt_list_new($1); }
  | stmt_list stmt               { $$ = ast_stmt_list_add($1, $2); }
  ;

stmt:
    expr ';'                     { $$ = $1; }
  | RETURN expr ';'             { $$ = ast_return_new($2); }
  | IF '(' expr ')' stmt        { $$ = ast_if_new($3, $5, NULL); }
  | IF '(' expr ')' stmt ELSE stmt { $$ = ast_if_new($3, $5, $7); }
  | WHILE '(' expr ')' stmt     { $$ = ast_while_new($3, $5); }
  | FOR '(' expr ';' expr ';' expr ')' stmt { $$ = ast_for_new($3, $5, $7, $9); }
  | DO stmt WHILE '(' expr ')' ';' { $$ = ast_do_while_new($2, $5); }
  | BREAK ';'                   { $$ = ast_break_new(); }
  | CONTINUE ';'                { $$ = ast_continue_new(); }
  | compound_stmt               { $$ = $1; }
  ;

expr:
    INTEGER                     { $$ = ast_int_new($1); }
  | STRING                      { $$ = ast_string_new($1); }
  | IDENT                       { $$ = ast_var_new($1); }
  | IDENT '=' expr              { $$ = ast_assign_new($1, $3); }
  | expr '+' expr               { $$ = ast_binop_new(OP_ADD, $1, $3); }
  | expr '-' expr               { $$ = ast_binop_new(OP_SUB, $1, $3); }
  | expr '*' expr               { $$ = ast_binop_new(OP_MUL, $1, $3); }
  | expr '/' expr               { $$ = ast_binop_new(OP_DIV, $1, $3); }
  | expr '%' expr               { $$ = ast_binop_new(OP_MOD, $1, $3); }
  | expr EQ expr                { $$ = ast_binop_new(OP_EQ, $1, $3); }
  | expr NEQ expr               { $$ = ast_binop_new(OP_NEQ, $1, $3); }
  | expr '<' expr               { $$ = ast_binop_new(OP_LT, $1, $3); }
  | expr '>' expr               { $$ = ast_binop_new(OP_GT, $1, $3); }
  | expr LE expr                { $$ = ast_binop_new(OP_LE, $1, $3); }
  | expr GE expr                { $$ = ast_binop_new(OP_GE, $1, $3); }
  | expr AND expr               { $$ = ast_binop_new(OP_AND, $1, $3); }
  | expr OR expr                { $$ = ast_binop_new(OP_OR, $1, $3); }
  | expr '&' expr               { $$ = ast_binop_new(OP_BITWISE_AND, $1, $3); }
  | expr '|' expr               { $$ = ast_binop_new(OP_BITWISE_OR, $1, $3); }
  | expr '^' expr               { $$ = ast_binop_new(OP_BITWISE_XOR, $1, $3); }
  | expr LSHIFT expr            { $$ = ast_binop_new(OP_LSHIFT, $1, $3); }
  | expr RSHIFT expr            { $$ = ast_binop_new(OP_RSHIFT, $1, $3); }
    | '-' expr %prec UNARY_MINUS  { $$ = ast_unary_new(OP_NEG, $2); }
    | '!' expr %prec UNARY_MINUS  { $$ = ast_unary_new(OP_NOT, $2); }
    | '~' expr %prec UNARY_MINUS  { $$ = ast_unary_new(OP_BITWISE_XOR, $2); }
  | '(' expr ')'                { $$ = $2; }
  | IDENT '[' expr ']'          { $$ = ast_array_access_new($1, $3); }
  | IDENT '(' ')'               { $$ = ast_call_new($1, NULL); }
  | IDENT '(' arg_list ')'      { $$ = ast_call_new($1, $3); }
  ;

arg_list:
    expr                        { $$ = ast_arg_list_new($1); }
  | arg_list ',' expr           { $$ = ast_arg_list_add($1, $3); }
  ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Parse error at line %d: %s\n", yylineno, s);
    exit(1);
}
