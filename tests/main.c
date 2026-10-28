#include <stdio.h>
#include <stdlib.h>
#include "test.h"

extern const TestCase __tests_start[];
extern const TestCase __tests_end[];

int main(void) {
    size_t passed = 0;
    size_t failed = 0;

    for (const TestCase *test = __tests_start; test < __tests_end; test++) {
        if (test->run()) {
            printf("[\x1b[32mPASS\x1b[0m] %s\n", test->name);
            passed++;
        } else {
            printf("[\x1b[31mFAIL\x1b[0m] %s\n", test->name);
            failed++;
        }
    }

    printf("\n%zu passed, %zu failed\n", passed, failed);

    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}