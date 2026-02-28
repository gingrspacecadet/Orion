#ifndef INCLUDE_XMALLOC_H
#define INCLUDE_XMALLOC_H

#include <stddef.h>

void *xmalloc(size_t size);
void *xcalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
void xfree(void *ptr);

#endif