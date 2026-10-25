#ifndef DA_H
#define DA_H

#include <stdlib.h>
#include <assert.h>

#if !defined(__GNUC__) && !defined(__clang__)
#   error This project requires GNU C extensions.
#endif

#if __STDC_VERSION__ < 201112L
#   error C11 required.
#endif

#define da_type(T)      \
    struct {            \
        T *data;        \
        size_t len;     \
        size_t cap;     \
    }

#define da_reserve(da, elems)                                                       \
    do {                                                                            \
        __auto_type _da = da;                                                       \
        size_t _n = elems;                                                          \
        if (_n <= _da->cap) break;                                                  \
        assert(_n <= SIZE_MAX / sizeof(*_da->data));                                \
        typeof(_da->data) new_ptr = realloc(_da->data, _n * sizeof(*_da->data));    \
        assert(new_ptr);                                                            \
        _da->data = new_ptr;                                                        \
        _da->cap = _n;                                                              \
    } while (0)

#define da_resize(da, elems)    \
    do {                        \
        __auto_type _da = da;   \
        da_reserve(_da, elems); \
        _da->len = elems;       \
    } while (0)                 \

#define da_free(da)                 \
    do {                            \
        __auto_type _da = da;       \
        free(_da->data);            \
        _da->data = NULL;           \
        _da->len  = 0;              \
        _da->cap  = 0;              \
    } while (0)

#define da_push(da, value)                              \
    do {                                                \
        __auto_type _da = da;                           \
        __auto_type _value = value;                   \
                                                        \
        if (_da->len == _da->cap)                       \
            da_resize(_da, _da->cap ? _da->cap * 2 : 16); \
                                                        \
        _da->data[_da->len++] = _value;                 \
    } while (0)

#define da_init(da) \
    do {                                             \
        __auto_type _da = da;                        \
        if (_da->data) break;                        \
        _da->data = malloc(2 * sizeof(*_da->data));  \
        if (!_da->data) break;                       \
        _da->len = 0;                                \
        _da->cap = 2;                                \
    } while (0)

#endif