#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>
#include <stdbool.h>
#include "source.h"

bool codegen(FILE *out, Source *source);

#endif