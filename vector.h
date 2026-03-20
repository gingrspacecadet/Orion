#ifndef _VECTOR_H
#define _VECTOR_H

#include <stdlib.h>
#include <stddef.h>

// preprocessorslop
#define VECTOR_DECLARE(T) \
typedef struct { \
    T *data; \
    size_t idx; \
    size_t cap; \
} T##Vector; \
static inline T##Vector T##Vector_init(void) { \
    T##Vector v = {calloc(8, sizeof(T)), 0, 8}; return v; \
} \
static inline void T##Vector_push(T##Vector *v, T item) { \
    if (v->idx == v->cap) { \
        v->cap = v->cap ? v->cap * 2 : 8; \
        v->data = realloc(v->data, v->cap * sizeof(T)); \
    } \
    v->data[v->idx++] = item; \
} \
static inline void T##Vector_free(T##Vector *v) { \
    free(v->data); \
    v->idx = v->cap = 0; \
} \
static inline T T##Vector_lookup(T##Vector *v, size_t idx) { \
    return v->data[idx > v->cap ? v->cap : idx]; \
} \
static inline void T##Vector_resize(T##Vector *v, size_t elems) { \
    if (elems < v->cap) return; \
    v->data = realloc(v->data, elems * sizeof(T)); \
    v->cap = elems; \
} \

#endif