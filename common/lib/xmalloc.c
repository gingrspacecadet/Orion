#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }
    
    return p;
}

void *xcalloc(size_t size) {
    void *p = xmalloc(size);
    memset(p, 0, size);
    return p;
}

void xfree(void *ptr) {
    if (!ptr) return;

    free(ptr);
}

void *xrealloc(void *ptr, size_t size) {
    if (!ptr) {
        fprintf(stderr, "Invalid ptr passed to xrealloc\n");
        exit(1);
    }

    xfree(ptr);
    return xmalloc(size);
}