/* Minimal single-header test harness.
 *
 *   #include "test.h"
 *   TEST(my_case) { CHECK(1 + 1 == 2); CHECK_EQ_INT(add(2, 2), 4); }
 *   int main(void) { RUN(my_case); return TEST_REPORT(); }
 */
#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

static int test_failures_ = 0;
static int test_checks_   = 0;
static int test_cur_failed_ = 0;

#define TEST(name) static void name(void)

#define CHECK(cond) do {                                                   \
        test_checks_++;                                                    \
        if (!(cond)) {                                                     \
            test_cur_failed_ = 1;                                          \
            fprintf(stderr, "  %s:%d: CHECK(%s) failed\n",                 \
                    __FILE__, __LINE__, #cond);                            \
        }                                                                  \
    } while (0)

#define CHECK_EQ_INT(a, b) do {                                            \
        long long a_ = (long long)(a), b_ = (long long)(b);                \
        test_checks_++;                                                    \
        if (a_ != b_) {                                                    \
            test_cur_failed_ = 1;                                          \
            fprintf(stderr, "  %s:%d: %s == %s failed (%lld vs %lld)\n",   \
                    __FILE__, __LINE__, #a, #b, a_, b_);                   \
        }                                                                  \
    } while (0)

#define CHECK_EQ_STR(a, b) do {                                            \
        const char *a_ = (a), *b_ = (b);                                   \
        test_checks_++;                                                    \
        if (strcmp(a_, b_) != 0) {                                         \
            test_cur_failed_ = 1;                                          \
            fprintf(stderr, "  %s:%d: %s == %s failed (\"%s\" vs \"%s\")\n", \
                    __FILE__, __LINE__, #a, #b, a_, b_);                   \
        }                                                                  \
    } while (0)

#define RUN(name) do {                                                     \
        test_cur_failed_ = 0;                                              \
        name();                                                            \
        if (test_cur_failed_) test_failures_++;                            \
        printf("[%s] %s\n", test_cur_failed_ ? "FAIL" : " OK ", #name);    \
    } while (0)

#define TEST_REPORT()                                                      \
    (printf("%s: %d check(s), %d failing test(s)\n",                       \
            __FILE__, test_checks_, test_failures_),                       \
     test_failures_ != 0)

#endif /* TEST_H */
