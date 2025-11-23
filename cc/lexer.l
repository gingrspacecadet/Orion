%{
#include "parser.h"
#include <stdlib.h>
#include <string.h>

%}

DIGIT   [0-9]
ID      [A-Za-z_][A-Za-z0-9_]*
WS      [ \t\r]+

%%

"int"           { return INT; }
"return"        { return RETURN; }
"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }

"=="            { return EQ; }
"!="            { return NEQ; }
"<="            { return LE; }
">="            { return GE; }
"&&"            { return AND; }
"||"            { return OR; }

{ID}            { yylval.string = strdup(yytext); return IDENT; }
{DIGIT}+        { yylval.ival = atoi(yytext); return INTEGER; }

"("             { return '('; }
")"             { return ')'; }
"{"             { return '{'; }
"}"             { return '}'; }
","             { return ','; }
";"             { return ';'; }
"+"             { return '+'; }
"-"             { return '-'; }
"*"             { return '*'; }
"/"             { return '/'; }
"="             { return '='; }
"<"             { return '<'; }
">"             { return '>'; }

\n              { ++yylineno; }
{WS}            { /* skip */ }

.               { return yytext[0]; }

%%

int yywrap(void) { return 1; }
