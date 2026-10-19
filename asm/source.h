#ifndef SOURCE_H
#define SOURCE_H

#include <stddef.h>
#include "token.h"

typedef struct {
    char *data;
    ptrdiff_t offset;
    size_t line;
} Source;

#endif