// ===== gin.h =====
// This is a header library that contains a bunch of 
// things i use a lot in C.
// feel free to use this in any project
// (although please credit me).
// enjoy!
// =================

// all functions, macros, and variables that begin with __gin_ are not indended for your use. please do not use them

#ifndef __GIN_H
#define __GIN_H

#define _GNU_SOURCE
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#undef _GNU_SOURCE

static inline void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "xmalloc: failed to allocate %zu bytes\n", size);
        exit(1);
    }
    return ptr;
}

static inline void *xcalloc(size_t size) {
    void *ptr = xmalloc(size);
    memset(ptr, 0, size);
    return ptr;
}

static inline void *xrealloc(void *ptr, size_t size) {
    ptr = realloc(ptr, size);
    if (!ptr) {
        fprintf(stderr, "xrealloc: failed to reallocate %zu bytes\n", size);
        exit(1);
    }
    return ptr;
}

// template instantiation logic

#define __GIN_VA_NARGS_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,N,...) N
#define __GIN_VA_NARGS(...) __GIN_VA_NARGS_IMPL(__VA_ARGS__,8,7,6,5,4,3,2,1)

#define __GIN_CONCAT_IMPL(A, B) A##B
#define __GIN_CONCAT(A, B) __GIN_CONCAT_IMPL(A, B)

#define __GIN_INSTANTIATE_SELECT(N) __GIN_CONCAT(__GIN_INSTANTIATE_, N)

#define __GIN_INSTANTIATE_2(A, M)               M(A)
#define __GIN_INSTANTIATE_3(A, B, M)            M(A, B)
#define __GIN_INSTANTIATE_4(A, B, C, M)         M(A, B, C)
#define __GIN_INSTANTIATE_5(A, B, C, D, M)      M(A, B, C, D)
#define __GIN_INSTANTIATE_6(A, B, C, D, E, M)   M(A, B, C, D, E)
#define __GIN_INSTANTIATE_7(A, B, C, D, E, F, M) M(A, B, C, D, E, F)
#define __GIN_INSTANTIATE_8(A, B, C, D, E, F, G, M) M(A, B, C, D, E, F, G)

// you don't have to use this, but i prefer it for consistency
// example: `INSTANTIATE(char *, chars, ARRAY_TEMPLATE)`
// it's equivalent to `ARRAY_TEMPLATE(char *, chars)`
// but i much prefer this
#define INSTANTIATE(...) __GIN_INSTANTIATE_SELECT(__GIN_VA_NARGS(__VA_ARGS__))(__VA_ARGS__)

// dynamic array template
#define ARRAY_TEMPLATE(T, N) \
typedef struct { \
    T *data; \
    size_t len; \
    size_t cap; \
    bool alive; \
} N##_array; \
static inline N##_array N##_array_empty(void) { return (N##_array){}; } \
static inline N##_array N##_array_init(void) { \
    N##_array v = {}; \
    v.data = (T*)xcalloc(sizeof(T)); \
    v.len = 0; \
    v.cap = 1; \
    v.alive = true; \
    return v; \
} \
static inline void N##_array_push(N##_array *v, T item) { \
    if (!v->alive) { \
        fprintf(stderr, #N "_array_push: uninitialised array\n"); \
        exit(1); \
    } \
    if (v->len == v->cap) { \
        v->cap = v->cap ? v->cap * 2 : 8; \
        v->data = (T*)realloc(v->data, v->cap * sizeof(T)); \
        if (!v->data) { \
            fprintf(stderr, #N "_array_push: realloc failed\n"); \
            exit(1); \
        } \
    } \
    v->data[v->len++] = item; \
} \
static inline void N##_array_append(N##_array *v, size_t num, T item) { \
    if (!v->alive) { \
        fprintf(stderr, #N "_array_append: uninitialised array\n"); \
        exit(1); \
    } \
    for (size_t i = 0; i < num; i++) { \
        N##_array_push(v, item); \
    } \
} \
static inline void N##_array_free(N##_array *v) { \
    if (!v->alive) { \
        fprintf(stderr, #N "_array_free: uninitialised array\n"); \
        exit(1); \
    } \
    free(v->data); \
    v->len = v->cap = 0; \
} \
static inline T N##_array_at(N##_array *v, size_t len) { \
    if (!v->alive) { \
        fprintf(stderr, #N "_array_at: uninitialised array\n"); \
        exit(1); \
    } \
    return v->data[len >= v->len ? (v->cap - 1) : len]; \
} \
static inline void N##_array_resize(N##_array *v, size_t elems) { \
    if (!v->alive) { \
        fprintf(stderr, #N "_array_resize: uninitialised array\n"); \
        exit(1); \
    } \
    if (elems < v->cap) return; \
    v->data = (T*)realloc(v->data, elems * sizeof(T)); \
    if (!v->data) { \
        fprintf(stderr, #N "_array_resize: realloc failed\n"); \
        exit(1); \
    } \
    v->cap = elems; \
} \
static inline void N##_array_clear(N##_array *v) { \
    if (!v->alive) { \
        fprintf(stderr, #N "_array_clear: uninitialised array\n"); \
        exit(1); \
    } \
    v->len = 0; \
} \

// very basic optional template. 
#define OPTIONAL_TEMPLATE(T, N) \
typedef struct N##_optional { \
    bool has_value; \
    T value; \
} N##_optional; \

static inline unsigned long __gin_hash(const char *str) {
    unsigned long h = 5381;
    int c;
    while ((c = *str++))
        h = ((h << 5) + h) + (unsigned char)c;
    return h;
}

// simple hashmap template. currently, all keys are strings but i will change that
#define HASHMAP_TEMPLATE(T, N) \
typedef struct N##_entry N##_entry; \
struct N##_entry { \
    char *key; \
    T value; \
    N##_entry *next; \
}; \
typedef struct N##_hashmap N##_hashmap; \
struct N##_hashmap { \
    N##_entry **buckets; \
    size_t cap; \
    size_t count; \
}; \
static inline N##_hashmap N##_hashmap_init(void) { \
    N##_hashmap hm = {0}; \
    hm.cap = 16; \
    hm.count = 0; \
    hm.buckets = (N##_entry**)calloc(hm.cap, sizeof(N##_entry*)); \
    return hm; \
} \
static inline void N##_hashmap_free(N##_hashmap *hm) { \
    if (!hm || !hm->buckets) return; \
    for (size_t i = 0; i < hm->cap; ++i) { \
        N##_entry *e = hm->buckets[i]; \
        while (e) { \
            N##_entry *next = e->next; \
            free(e->key); \
            free(e); \
            e = next; \
        } \
    } \
    free(hm->buckets); \
    hm->buckets = NULL; \
    hm->cap = hm->count = 0; \
} \
static inline void N##_hashmap_rehash(N##_hashmap *hm, size_t newcap) { \
    N##_entry **old = hm->buckets; \
    size_t oldcap = hm->cap; \
    hm->buckets = (N##_entry**)calloc(newcap, sizeof(N##_entry*)); \
    hm->cap = newcap; \
    for (size_t i = 0; i < oldcap; ++i) { \
        N##_entry *e = old[i]; \
        while (e) { \
            N##_entry *next = e->next; \
            unsigned long h = __gin_hash(e->key) % hm->cap; \
            e->next = hm->buckets[h]; \
            hm->buckets[h] = e; \
            e = next; \
        } \
    } \
    free(old); \
} \
static inline void N##_hashmap_put(N##_hashmap *hm, const char *key, T value) { \
    if (!hm) return; \
    if (hm->count > hm->cap * 1.5) { \
        N##_hashmap_rehash(hm, hm->cap * 2); \
    } \
    unsigned long h = __gin_hash(key) % hm->cap; \
    N##_entry *e = hm->buckets[h]; \
    while (e) { \
        if (strcmp(e->key, key) == 0) { \
            e->value = value; \
            return; \
        } \
        e = e->next; \
    } \
    N##_entry *ne = (N##_entry*)xmalloc(sizeof(N##_entry)); \
    ne->key = xstrdup(key); \
    ne->value = value; \
    ne->next = hm->buckets[h]; \
    hm->buckets[h] = ne; \
    hm->count++; \
} \
static inline T N##_hashmap_get(N##_hashmap *hm, const char *key) { \
    T nullv = (T)0; \
    if (!hm || !hm->buckets) return nullv; \
    unsigned long h = __gin_hash(key) % hm->cap; \
    N##_entry *e = hm->buckets[h]; \
    while (e) { \
        if (strcmp(e->key, key) == 0) return e->value; \
        e = e->next; \
    } \
    return nullv; \
} \
static inline int N##_hashmap_remove(N##_hashmap *hm, const char *key) { \
    if (!hm || !hm->buckets) return 0; \
    unsigned long h = __gin_hash(key) % hm->cap; \
    N##_entry *e = hm->buckets[h]; \
    N##_entry *prev = NULL; \
    while (e) { \
        if (strcmp(e->key, key) == 0) { \
            if (prev) prev->next = e->next; else hm->buckets[h] = e->next; \
            free(e->key); \
            free(e); \
            hm->count--; \
            return 1; \
        } \
        prev = e; \
        e = e->next; \
    } \
    return 0; \
}

// better strings! they are ptr+len pairs

typedef struct {
    char *ptr;
    size_t len;
} String;

// use this in printf-style functions
#define string_fmt(S) (int)S.len, S.ptr

// bounds-checked string accessing
static inline char string_at(String string, size_t index) {
    if (index >= string.len) {
        fprintf(stderr, "string_at: OOB access\n");
        exit(1);
    }
    return string.ptr[index];
}

// lets you have comptime and runtime strings
#define string_make(cstr) (String){ \
        .ptr = (cstr), \
        .len = (__builtin_constant_p(__builtin_strlen(cstr)) \
                   ? (sizeof(cstr) - 1) \
                   : strlen(cstr)) \
    }

static inline bool string_eq(String a, String b) {
    if (a.len != b.len) return false;
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

static inline int string_cmp(String a, String b) {
    if (a.len != b.len) return (a.len < b.len) ? -1 : 1;
    return memcmp(a.ptr, b.ptr, a.len);
}

static inline String string_find(String str, String needle) {
    if (needle.len > str.len) return (String){0};
    char *found = memmem(str.ptr, str.len, needle.ptr, needle.len);
    if (found) {
        return (String) {
            .ptr = found,
            .len = str.len - (found - str.ptr)
        };
    } else {
        return (String){0};
    }
}

static inline bool string_contains(String str, String needle) {
    String found = string_find(str, needle);
    if (found.ptr) return true;
    else return false;
}

// in-place cstring formatter
static inline char *format(char *msg, ...) {
    char *buf;
    va_list args;
    va_start(args, msg);
    int n = vasprintf(&buf, msg, args);
    if (n == -1) return msg;
    else return buf;
}

// custom allocators

typedef struct {
    void **blocks;
    size_t block_count;
    size_t block_size;
    size_t current_index;
} Arena;

#define __GIN_ARENA_BLOCK_SIZE (1024 * 1024)

static inline Arena *arena_create() {
    Arena *a = xmalloc(sizeof(Arena));
    if (!a) {
        exit(1);
    }
    a->block_size = __GIN_ARENA_BLOCK_SIZE;
    a->block_count = 1;
    a->blocks = xmalloc(sizeof(void*));
    a->blocks[0] = xmalloc(a->block_size);
    a->current_index = 0;
    return a;
}

static inline void *arena_alloc(Arena *a, size_t size) {
    if (size > a->block_size - a->current_index) {
        a->block_count++;
        a->blocks = realloc(a->blocks, a->block_count * sizeof(void*));
        if (!a->blocks) {
            exit(1);
        }
        a->blocks[a->block_count - 1] = xmalloc(a->block_size);
        a->current_index = 0;
    }

    void *data = (char*)a->blocks[a->block_count - 1] + a->current_index;
    a->current_index += size;
    return data;
}

static inline void *arena_calloc(Arena *a, size_t size) {
    void *data = arena_alloc(a, size);
    memset(data, 0, size);
    return data;
}

static inline char *arena_strdup(Arena *a, char *s) {
    size_t n = strlen(s) + 1;
    char *dst = arena_alloc(a, n);
    memcpy(dst, s, n);
    return dst;
}

static inline void arena_destroy(Arena *a) {
    for (size_t i = 0; i < a->block_count; i++) {
        free(a->blocks[i]);
    }
    free(a->blocks);
    free(a);
}

#endif