#ifndef ORION_TEST_H
#define ORION_TEST_H

#include <stdbool.h>
#include <stdio.h>

typedef bool (*TestFn)(void);

typedef struct {
    const char *name;
    TestFn run;
} TestCase;

#define TEST(id)                                                        \
    static bool test_##id(void);                                        \
    static const TestCase test_record_##id                              \
        __attribute__((used, section("tests"),                          \
                       aligned(__alignof__(TestCase)))) = {             \
            .name = #id,                                                \
            .run = test_##id,                                           \
        };                                                              \
    static bool test_##id(void)

#define CHECK(condition)                                                \
    do {                                                                \
        if (!(condition)) {                                             \
            fprintf(stderr,                                             \
                    "      %s:%d: CHECK(%s) failed\n",                  \
                    __FILE__, __LINE__, #condition);                    \
            return false;                                               \
        }                                                               \
    } while (0)

#endif