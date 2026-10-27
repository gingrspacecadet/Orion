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

#define da_reserve(da, elems)                                                   \
    do {                                                                        \
        __auto_type _da_reserve = (da);                                         \
        size_t _da_reserve_n = (elems);                                         \
        if (_da_reserve_n <= _da_reserve->cap) break;                           \
        assert(_da_reserve_n <= SIZE_MAX / sizeof(*_da_reserve->data));         \
        typeof(_da_reserve->data) _da_reserve_ptr =                             \
            realloc(_da_reserve->data,                                          \
                    _da_reserve_n * sizeof(*_da_reserve->data));                \
        assert(_da_reserve_ptr);                                                \
        _da_reserve->data = _da_reserve_ptr;                                    \
        _da_reserve->cap = _da_reserve_n;                                       \
    } while (0)

#define da_resize(da, elems)                                                    \
    do {                                                                        \
        __auto_type _da_resize = (da);                                          \
        size_t _da_resize_n = (elems);                                          \
        da_reserve(_da_resize, _da_resize_n);                                   \
        _da_resize->len = _da_resize_n;                                         \
    } while (0)

#define da_free(da)                 \
    do {                            \
        __auto_type _da = da;       \
        free(_da->data);            \
        _da->data = NULL;           \
        _da->len  = 0;              \
        _da->cap  = 0;              \
    } while (0)

#define da_push(da, value)                                                      \
    do {                                                                        \
        __auto_type _da_push = (da);                                            \
        __auto_type _da_push_value = (value);                                   \
        if (_da_push->len == _da_push->cap) {                                   \
            size_t _da_push_cap =                                               \
                _da_push->cap ? _da_push->cap * 2 : 16;                         \
            da_reserve(_da_push, _da_push_cap);                                 \
        }                                                                       \
        _da_push->data[_da_push->len++] = _da_push_value;                       \
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