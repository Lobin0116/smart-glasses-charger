#ifndef TEST_ASSERT_H
#define TEST_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

static int test_total = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        test_total++;                                                          \
        if (!(cond)) {                                                         \
            test_failed++;                                                     \
            printf("  FAIL: %s (line %d): %s\n", __func__, __LINE__, msg);     \
            return;                                                            \
        }                                                                      \
        test_passed++;                                                         \
    } while (0)

#define TEST_ASSERT_EQ(a, b, msg)                                              \
    do {                                                                       \
        test_total++;                                                          \
        if ((a) != (b)) {                                                      \
            test_failed++;                                                     \
            printf("  FAIL: %s (line %d): %s (got %ld, expected %ld)\n",       \
                   __func__, __LINE__, msg, (long)(a), (long)(b));             \
            return;                                                            \
        }                                                                      \
        test_passed++;                                                         \
    } while (0)

#define TEST_RUN(func)                                                         \
    do {                                                                       \
        printf("-- %s\n", #func);                                              \
        func();                                                                \
    } while (0)

#define TEST_SUMMARY()                                                         \
    do {                                                                       \
        printf("\n========================================\n");                \
        printf("Total: %d  Passed: %d  Failed: %d\n",                         \
               test_total, test_passed, test_failed);                          \
        printf("========================================\n");                  \
        return (test_failed > 0) ? 1 : 0;                                      \
    } while (0)

#endif
