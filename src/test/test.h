#pragma once
/**
 * @file
 * @brief Test: a small unit-test harness that catches leaks, hangs and
 * out-of-memory bugs.
 *
 * A test file is a program. Include this header first, then the `.c` files
 * under test, then write tests and a main that runs them:
 *
 * @code
 * #include "test/test.h"
 *
 * #include "log/log.c"
 * #include "mem/mem.c"
 * #include "str/str.c"
 *
 * TEST(appends) {
 *     Str s = { 0 };
 *     REQUIRE(str_append(&s, SV("mule")));
 *     CHECK_EQ_STR(str_cstr(&s), "mule");
 *     str_free(&s);
 * }
 *
 * int main(int argc, char **argv) {
 *     test_init(argc, argv);
 *     RUN(appends);
 *     return TEST_REPORT();
 * }
 * @endcode
 *
 *   - CHECK records a failure and carries on. REQUIRE records it and returns
 *     from the test, for when the rest of it would be meaningless.
 *   - Every file included after this one has malloc, calloc, realloc and free
 *     redirected to counting wrappers. RUN fails a test that ends with more
 *     live allocations than it started with, so leaks fail on every platform.
 *   - test_fail_alloc_after() makes allocation fail on demand, so
 *     out-of-memory paths can be tested deterministically.
 *   - A test still running after #TEST_TIMEOUT_S seconds is reported as
 *     `[HANG]` and fails the run.
 *   - Pass test names as arguments to run only those tests.
 *   - Results are coloured when stdout supports it, as log_color_enabled()
 *     decides. The harness needs the log module, and log needs mem and str.
 */
#include "base/base.h" /* first: it sets the POSIX feature macros */
#include "log/log.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef TEST_TIMEOUT_S
/** Seconds before a running test counts as hung. Define it before the
 *  include to change it. */
#define TEST_TIMEOUT_S 10
#endif

/** @cond */
static int test_failures_ = 0;
static int test_checks_ = 0;
static int test_cur_failed_ = 0;
static int test_argc_ = 0;
static char **test_argv_ = NULL;
static const char *test_running_ = "";
static bool test_color_ = false; /* set once by test_init */

/* --- Allocator hooks ----------------------------------------------------- */

static long test_live_allocs_ = 0;
static long test_alloc_budget_ = -1; /* allocations left; -1 means no limit */

/** @endcond */

/** Let the next @p n allocations succeed and fail every one after. Pass -1
 *  to stop failing. RUN resets it after each test. */
MULE_UNUSED static void test_fail_alloc_after(long n) {
    test_alloc_budget_ = n;
}

/** @cond */

static bool test_alloc_allowed_(void) {
    if (test_alloc_budget_ == 0) return false;
    if (test_alloc_budget_ > 0) test_alloc_budget_--;
    return true;
}

MULE_UNUSED static void *test_malloc_(size_t n) {
    if (!test_alloc_allowed_()) return NULL;
    void *p = malloc(n);
    if (p != NULL) test_live_allocs_++;
    return p;
}

MULE_UNUSED static void *test_calloc_(size_t count, size_t n) {
    if (!test_alloc_allowed_()) return NULL;
    void *p = calloc(count, n);
    if (p != NULL) test_live_allocs_++;
    return p;
}

MULE_UNUSED static void *test_realloc_(void *old, size_t n) {
    if (!test_alloc_allowed_()) return NULL;
    void *p = realloc(old, n);
    if (p != NULL && old == NULL) test_live_allocs_++;
    return p;
}

MULE_UNUSED static void test_free_(void *p) {
    if (p != NULL) test_live_allocs_--;
    free(p);
}

#define malloc test_malloc_
#define calloc test_calloc_
#define realloc test_realloc_
#define free test_free_

/* --- Checks -------------------------------------------------------------- */

static void test_fail_at_(const char *file, int line) {
    test_cur_failed_ = 1;
    if (test_color_)
        printf("  " LOG_RED "%s:%d:" LOG_RESET " ", file, line);
    else
        printf("  %s:%d: ", file, line);
}

/* Print bytes as a C string literal, escaping anything unprintable. */
static void test_print_bytes_(const char *p, size_t n) {
    if (p == NULL) {
        printf("null");
        return;
    }
    putchar('"');
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c >= 0x20 && c < 0x7f && c != '"' && c != '\\')
            putchar(c);
        else
            printf("\\x%02x", c);
    }
    putchar('"');
}

MULE_UNUSED static bool test_check_(bool ok, const char *file, int line,
                                    const char *expr) {
    test_checks_++;
    if (!ok) {
        test_fail_at_(file, line);
        printf("CHECK(%s) failed\n", expr);
    }
    return ok;
}

MULE_UNUSED static bool test_eq_int_(long long a, long long b, const char *file,
                                     int line, const char *ea, const char *eb) {
    test_checks_++;
    if (a != b) {
        test_fail_at_(file, line);
        printf("%s == %s failed (%lld vs %lld)\n", ea, eb, a, b);
    }
    return a == b;
}

MULE_UNUSED static bool test_eq_str_(const char *a, const char *b,
                                     const char *file, int line, const char *ea,
                                     const char *eb) {
    bool ok = (a == NULL || b == NULL) ? a == b : strcmp(a, b) == 0;
    test_checks_++;
    if (!ok) {
        test_fail_at_(file, line);
        printf("%s == %s failed (", ea, eb);
        test_print_bytes_(a, a ? strlen(a) : 0);
        printf(" vs ");
        test_print_bytes_(b, b ? strlen(b) : 0);
        printf(")\n");
    }
    return ok;
}

MULE_UNUSED static bool test_eq_mem_(const char *a, size_t alen, const char *b,
                                     size_t blen, const char *file, int line,
                                     const char *ea, const char *eb) {
    bool ok = alen == blen && (alen == 0 || memcmp(a, b, alen) == 0);
    test_checks_++;
    if (!ok) {
        test_fail_at_(file, line);
        printf("%s == %s failed (", ea, eb);
        test_print_bytes_(a, alen);
        printf(" vs ");
        test_print_bytes_(b, blen);
        printf(")\n");
    }
    return ok;
}

/** @endcond */

/** Define a test. Run it from main with RUN. */
#define TEST(name) static void name(void)

/** Record a failure when @p cond is false, and carry on. */
#define CHECK(cond) ((void)test_check_((cond), __FILE__, __LINE__, #cond))

/** Record a failure when @p cond is false, and return from the test. */
#define REQUIRE(cond)                                                          \
    do {                                                                       \
        if (!test_check_((cond), __FILE__, __LINE__, #cond)) return;           \
    } while (0)

/** CHECK that two integers are equal, printing both values when not. */
#define CHECK_EQ_INT(a, b)                                                     \
    ((void)test_eq_int_((long long)(a), (long long)(b), __FILE__, __LINE__,    \
                        #a, #b))

/** CHECK that two C strings are equal. Two nulls are equal, a null and a
 *  string are not. */
#define CHECK_EQ_STR(a, b)                                                     \
    ((void)test_eq_str_((a), (b), __FILE__, __LINE__, #a, #b))

/** CHECK that two byte ranges have the same length and bytes, embedded NULs
 *  included. */
#define CHECK_EQ_MEM(a, alen, b, blen)                                         \
    ((void)test_eq_mem_((a), (alen), (b), (blen), __FILE__, __LINE__, #a, #b))

/* --- Running ------------------------------------------------------------- */

/** @cond */
/* SIGALRM handler, so only async-signal-safe calls: write and _exit. */
static void test_timeout_(int sig) {
    (void)sig;
    static const char plain[] = "[HANG] ";
    static const char color[] = LOG_YELLOW "[HANG]" LOG_RESET " ";
    if (test_color_)
        write(STDOUT_FILENO, color, sizeof color - 1);
    else
        write(STDOUT_FILENO, plain, sizeof plain - 1);
    write(STDOUT_FILENO, test_running_, strlen(test_running_));
    write(STDOUT_FILENO, "\n", 1);
    _exit(EXIT_FAILURE);
}

/** @endcond */

/** Start a test program. Call it first in main with main's arguments. */
MULE_UNUSED static void test_init(int argc, char **argv) {
    test_argc_ = argc;
    test_argv_ = argv;
    test_color_ = log_color_enabled(stdout);
    signal(SIGALRM, test_timeout_);
}

/** @cond */
static void test_print_result_(const char *name) {
    const char *tag = test_cur_failed_ ? "FAIL" : " OK ";
    if (test_color_)
        printf("%s[%s]" LOG_RESET " %s\n",
               test_cur_failed_ ? LOG_RED : LOG_GREEN, tag, name);
    else
        printf("[%s] %s\n", tag, name);
}

MULE_UNUSED static int test_report_(const char *file) {
    const char *on = test_color_ ? (test_failures_ ? LOG_RED : LOG_GREEN) : "";
    printf("%s%s: %d check(s), %d failing test(s)%s\n", on, file, test_checks_,
           test_failures_, test_color_ ? LOG_RESET : "");
    return test_failures_ != 0;
}

static bool test_selected_(const char *name) {
    if (test_argc_ <= 1) return true;
    for (int i = 1; i < test_argc_; i++) {
        if (strcmp(test_argv_[i], name) == 0) return true;
    }
    return false;
}

/** @endcond */

/** Run a test unless arguments name others, then print `[ OK ]` or `[FAIL]`.
 *  A leak or a hang fails the test too. */
#define RUN(name)                                                              \
    do {                                                                       \
        if (!test_selected_(#name)) break;                                     \
        long live_before_ = test_live_allocs_;                                 \
        test_cur_failed_ = 0;                                                  \
        test_running_ = #name;                                                 \
        alarm(TEST_TIMEOUT_S);                                                 \
        name();                                                                \
        alarm(0);                                                              \
        test_alloc_budget_ = -1;                                               \
        if (test_live_allocs_ != live_before_) {                               \
            test_cur_failed_ = 1;                                              \
            printf("  %ld allocation(s) leaked\n",                             \
                   test_live_allocs_ - live_before_);                          \
        }                                                                      \
        if (test_cur_failed_) test_failures_++;                                \
        test_print_result_(#name);                                             \
        fflush(stdout); /* keep finished results if a later test hangs */      \
    } while (0)

/** Print the totals. Return it from main: it is nonzero when a test failed. */
#define TEST_REPORT() test_report_(__FILE__)
