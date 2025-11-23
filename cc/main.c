#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

extern int yyparse(void);
extern FILE *yyin;
void codegen_all(FILE *out);

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <source.c> [-o out.s]\n", argv[0]);
        return 1;
    }
    yyin = fopen(argv[1], "r");
    if (!yyin) { perror("fopen"); return 1; }
    yyparse();
    const char *outname = "out.s";
    FILE *out = fopen(outname, "w");
    if (!out) { perror("fopen out"); return 1; }
    codegen_all(out);
    fclose(out);
    printf("Wrote assembly to %s\n", outname);
    return 0;
}
