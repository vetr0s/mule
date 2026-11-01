/* MULE: Multi-Use Library Essentials, as one header.
 *
 * Generated from src/ by `make dist`. Edit src/ and regenerate.
 *
 * Include it anywhere for the declarations. In exactly one .c file, define
 * MULE_IMPLEMENTATION first to compile the library into that file:
 *
 *     #define MULE_IMPLEMENTATION
 *     #include "mule.h"
 *
 * Include it before any system header. It sets _POSIX_C_SOURCE, which only
 * takes effect before the first one.
 *
 * Define MULE_TEST as well to get the test harness. Its leak counting and
 * allocation failure cover the library only in the MULE_IMPLEMENTATION file,
 * so define both in the test program.
 */
#ifndef MULE_H
#define MULE_H

/* --- src/base/base.h --- */

/**
 * @file
 * @brief Base: what every MULE module needs, included before anything else.
 *
 * Layout rules for the library:
 *   - One folder per module under src/, holding `name.h` and `name.c`. A
 *     module of only macros and static functions is `name.h` alone.
 *   - Every header includes `base/base.h` first, and include paths are
 *     relative to src/ (`#include "str/str.h"`).
 *   - A `.c` file includes headers only, never another `.c`. Each program's
 *     unity file includes every `.c` it needs. Any order works, because
 *     each `.c` includes the headers it uses, so clang-format may sort them.
 *   - The whole program is one translation unit, so `static` does not keep
 *     names apart. Static helpers carry the module prefix and a trailing `_`.
 *   - Something belongs here only once two modules use it.
 *
 * Targets are Unix with clang 17+ or gcc 13+.
 */

/** @cond */
/* Feature macros only take effect before the first system header. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stddef.h>
#include <stdint.h>

#if __has_include(<stdckdint.h>)
#include <stdckdint.h>
#else
/* gcc 13 has the builtins but not the C23 header. */
#define ckd_add(r, a, b) __builtin_add_overflow((a), (b), (r))
#define ckd_sub(r, a, b) __builtin_sub_overflow((a), (b), (r))
#define ckd_mul(r, a, b) __builtin_mul_overflow((a), (b), (r))
#endif
/** @endcond */

/* --- src/mem/mem.h --- */

/**
 * @file
 * @brief Mem: the allocator interface and an arena.
 *
 *   - An Allocator is one realloc-shaped function plus a context pointer.
 *     Modules that allocate take a `const Allocator *`, and null means the
 *     libc heap.
 *   - An Arena hands out memory from large blocks and frees it all at once.
 *     A zero-initialised Arena (`Arena a = {};`) is valid and owns nothing.
 *   - Growing the most recent arena allocation happens in place when the
 *     block has room, so a Str growing in an arena does not leave a copy
 *     behind at every doubling.
 *
 * @snippet{trimleft} test_mem.c arena
 */


/** Allocate, resize or free, depending on @p old and @p new_size. */
typedef void *AllocatorFn(void *ctx, void *old, size_t old_size,
                          size_t new_size);

/** Where a module gets its memory. */
typedef struct Allocator {
    AllocatorFn *fn; /**< Called with `ctx` and the mem_realloc arguments. */
    void *ctx;       /**< Passed to `fn` unchanged. */
} Allocator;

/** Resize @p old from @p old_size to @p new_size bytes through @p a.
 *  Null @p old allocates and a @p new_size of 0 frees. Null @p a means the
 *  libc heap. Returns null on failure, leaving @p old untouched. */
[[nodiscard]] void *mem_realloc(const Allocator *a, void *old, size_t old_size,
                                size_t new_size);

/** Bytes in a regular arena block. Larger requests get a block of their own. */
#define MEM_ARENA_BLOCK ((size_t)64 * 1024)

/** One block of an arena. Defined in mem.c. */
typedef struct ArenaBlock ArenaBlock;

/** Bump allocator over a chain of blocks. Do not copy or move an Arena once
 *  arena_allocator() has been called on it. */
typedef struct Arena {
    ArenaBlock *head;    /**< Newest block. Older blocks chain behind it. */
    void *last;          /**< Most recent allocation, for in-place growth. */
    Allocator allocator; /**< Filled by arena_allocator(). */
} Arena;

/** @p size bytes aligned to @p align, a power of two. Null on failure. */
[[nodiscard]] void *arena_alloc(Arena *a, size_t size, size_t align);
/** An Allocator that draws from @p a. It lives as long as @p a.
 *  @snippet{trimleft} test_mem.c allocator */
[[nodiscard]] const Allocator *arena_allocator(Arena *a);
/** Forget every allocation and keep the newest block for reuse. */
void arena_reset(Arena *a);
/** Release every block. Safe to call twice, and @p a stays usable. */
void arena_free(Arena *a);

/* --- src/str/str.h --- */

/**
 * @file
 * @brief Str: an owned, growable byte string.
 *
 *   - `data` is always NUL-terminated when non-null, so it can be passed to
 *     C APIs, but `len` is authoritative (embedded NULs are allowed).
 *   - `capacity` is the number of bytes usable for content, excluding the
 *     terminator (the allocation is `capacity + 1` bytes).
 *   - A zero-initialised Str (`Str s = {};`) is a valid empty string and owns
 *     no memory; every function accepts it.
 *   - `alloc` says where the bytes live. Null means the libc heap. Set it
 *     before the first allocation and leave it alone after; str_free keeps it.
 *   - Functions that may allocate return `bool` (false = out of memory, size
 *     overflow, or a read error). On failure the string is left unchanged.
 *
 * Examples on this page are excerpts of tests/test_str.c and run with
 * `make test`.
 *
 * @snippet{trimleft} test_str.c build
 *
 * @snippet{trimleft} test_str.c arena_str
 */


#include <stdarg.h>
#include <stdio.h>

/** Owned, growable byte string. Zero-initialise before first use. */
typedef struct Str {
    char *data;      /**< Buffer, or null when nothing is allocated. */
    size_t len;      /**< Bytes of content, excluding the terminator. */
    size_t capacity; /**< Content bytes that fit before the next realloc. */
    const Allocator *alloc; /**< Source of `data`. Null means the heap. */
} Str;

/** Non-owning view of bytes; handy for passing slices and literals around. */
typedef struct StrView {
    const char *data; /**< First byte. Not necessarily NUL-terminated. */
    size_t len;       /**< Number of bytes in the view. */
} StrView;

/** View of a string literal, with its length computed at compile time. */
#define SV(lit) ((StrView){ .data = (lit), .len = sizeof(lit) - 1 })
/** printf helper: `printf("%.*s", STR_FMT(s))` prints exactly len bytes.
 *  @snippet{trimleft} test_str.c fmt */
#define STR_FMT(s) (int)(s).len, (s).data

/* --- Lifetime ------------------------------------------------------------ */

/** Initialise @p s as empty with room for @p capacity bytes of content. */
[[nodiscard]] bool str_with_capacity(Str *s, size_t capacity);
/** Initialise @p s as a copy of @p v. */
[[nodiscard]] bool str_from(Str *s, StrView v);
/** Initialise @p s as a copy of @p cstr. Null gives an empty string. */
[[nodiscard]] bool str_from_cstr(Str *s, const char *cstr);
/** Free the buffer and reset @p s to the zero state. Safe to call twice.
 *  @snippet{trimleft} test_str.c free */
void str_free(Str *s);

/* --- Capacity ------------------------------------------------------------ */

/** Ensure @p additional more bytes fit without another allocation. */
[[nodiscard]] bool str_reserve(Str *s, size_t additional);
/** Shrink the allocation to exactly `len + 1` bytes. */
[[nodiscard]] bool str_shrink_to_fit(Str *s);
/** Set the length to zero. Keeps the allocation for reuse.
 *  @snippet{trimleft} test_str.c clear */
void str_clear(Str *s);

/* --- Mutation ------------------------------------------------------------ */

/** Append the single byte @p c. */
[[nodiscard]] bool str_push(Str *s, char c);
/** Append @p v. @p v may point into @p s itself.
 *  @snippet{trimleft} test_str.c self_append */
[[nodiscard]] bool str_append(Str *s, StrView v);
/** Append @p cstr. A null @p cstr appends nothing. */
[[nodiscard]] bool str_append_cstr(Str *s, const char *cstr);
/** Append the printf-style expansion of @p fmt.
 *  @snippet{trimleft} test_str.c appendf */
[[nodiscard]] bool str_appendf(Str *s, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
/** str_appendf for a `va_list`, like vprintf. @p ap is used up afterwards. */
[[nodiscard]] bool str_vappendf(Str *s, const char *fmt, va_list ap)
    __attribute__((format(printf, 2, 0)));
/** Append everything left to read from @p f. On false, check ferror(f).
 *  @snippet{trimleft} test_str.c append_file */
[[nodiscard]] bool str_append_file(Str *s, FILE *f);
/** Replace the contents of @p s with the next line of @p f, without its
 *  `\n`. A `\r` before it is kept, and embedded NULs are allowed. False at
 *  end of input, on a read error (check ferror(f)) or out of memory. On false
 *  the contents of @p s are unchanged, though its capacity may have grown.
 *  @snippet{trimleft} test_str.c read_line */
[[nodiscard]] bool str_read_line(Str *s, FILE *f);

/* --- Access -------------------------------------------------------------- */

/** Borrow the whole of @p s. Invalidated by any call that may reallocate. */
[[nodiscard]] StrView str_view(const Str *s);
/** NUL-terminated contents of @p s. Never null, even for an empty Str. */
[[nodiscard]] const char *str_cstr(const Str *s);
/** True when @p a and @p b hold the same bytes.
 *  @snippet{trimleft} test_str.c eq */
[[nodiscard]] bool str_eq(StrView a, StrView b);
/** View of @p cstr, measured with strlen. Null gives an empty view. */
[[nodiscard]] StrView sv_from_cstr(const char *cstr);

/* --- Views --------------------------------------------------------------- */

/** Returned by the find functions when there is no match. */
#define SV_NPOS SIZE_MAX

/** Index of the first @p c in @p v, or SV_NPOS. */
[[nodiscard]] size_t sv_find_byte(StrView v, char c);
/** Index of the first @p needle in @p v, or SV_NPOS. An empty needle is at 0.
 *  @snippet{trimleft} test_str.c find */
[[nodiscard]] size_t sv_find(StrView v, StrView needle);
/** Byte order like memcmp: negative, zero or positive. A prefix sorts first. */
[[nodiscard]] int sv_cmp(StrView a, StrView b);
/** True when @p v begins with @p prefix. */
[[nodiscard]] bool sv_starts_with(StrView v, StrView prefix);
/** True when @p v ends with @p suffix. */
[[nodiscard]] bool sv_ends_with(StrView v, StrView suffix);
/** Bytes @p from up to but not including @p to. Asserts the range is in @p v.
 */
[[nodiscard]] StrView sv_slice(StrView v, size_t from, size_t to);
/** @p v without leading and trailing ASCII whitespace. */
[[nodiscard]] StrView sv_trim(StrView v);
/** Cut the piece before @p sep off @p rest. `a,,b,` splits as a, "", b.
 *  @snippet{trimleft} test_str.c split */
[[nodiscard]] bool sv_split_next(StrView *rest, StrView sep, StrView *piece);
/** strtok without the mutation: skip @p delims, cut the next non-empty run.
 *  @snippet{trimleft} test_str.c token */
[[nodiscard]] bool sv_token_next(StrView *rest, StrView delims, StrView *token);

/* --- Parsing ------------------------------------------------------------- */

/** Parse all of @p v as a base-10 integer with an optional sign. Whitespace,
 *  trailing bytes and overflow fail. On false @p out is unchanged.
 *  @snippet{trimleft} test_str.c parse */
[[nodiscard]] bool sv_parse_i64(StrView v, int64_t *out);
/** Parse all of @p v as a double, in any form strtod accepts in the C locale.
 *  Whitespace, trailing bytes, overflow to infinity and views over 255 bytes
 *  fail. On false @p out is unchanged. */
[[nodiscard]] bool sv_parse_f64(StrView v, double *out);

/* --- src/arr/arr.h --- */

/**
 * @file
 * @brief Arr: a growable array of any element type.
 *
 *   - Name each array type once with a typedef: `typedef Arr(int) IntArr;`.
 *     Every spelling of `Arr(...)` is a distinct type, so use the name.
 *   - A zero-initialised array (`IntArr xs = {};`) is valid, empty, and owns
 *     no memory.
 *   - The layout matches Str: `data`, `len`, `capacity` and `alloc`, where a
 *     null `alloc` means the libc heap. Index `data` directly to read and
 *     write elements, and pass it to qsort or memcpy.
 *   - Calls that may allocate return `bool`. On false the array is unchanged.
 *   - Growing may move `data`, so pointers into it do not survive arr_push,
 *     arr_insert or arr_reserve.
 *   - The macros evaluate the array argument more than once. Pass a variable
 *     or a plain pointer, never an expression with side effects.
 *
 * @snippet{trimleft} test_arr.c arr
 */


#include <assert.h>
#include <string.h>

/** A growable array of @p T. Declare it with a typedef before use. */
#define Arr(T)                                                                 \
    struct {                                                                   \
        T *data;                                                               \
        size_t len;                                                            \
        size_t capacity;                                                       \
        const Allocator *alloc;                                                \
    }

/** Make room for @p n more elements without another allocation. */
#define arr_reserve(a, n)                                                      \
    arr_reserve_(&(a)->data, &(a)->capacity, (a)->len, (n), sizeof *(a)->data, \
                 (a)->alloc)

/** Append @p v. @p v is evaluated after any growth, so it may read the array
 *  itself, as in `arr_push(&xs, xs.data[0])`. */
#define arr_push(a, v)                                                         \
    (arr_reserve((a), 1) ? ((a)->data[(a)->len++] = (v), true) : false)

/** Insert @p v before index @p i, shifting the rest up. @p i may equal len. */
#define arr_insert(a, i, v)                                                    \
    (assert((i) <= (a)->len),                                                  \
     arr_reserve((a), 1) ? (memmove((a)->data + (i) + 1, (a)->data + (i),      \
                                    ((a)->len - (i)) * sizeof *(a)->data),     \
                            (a)->data[(i)] = (v), (a)->len++, true)            \
                         : false)

/** Remove the element at index @p i, shifting the rest down. */
#define arr_remove(a, i)                                                       \
    (assert((i) < (a)->len),                                                   \
     memmove((a)->data + (i), (a)->data + (i) + 1,                             \
             ((a)->len - (i) - 1) * sizeof *(a)->data),                        \
     (void)(a)->len--)

/** Remove and return the last element. The array must not be empty. */
#define arr_pop(a) (assert((a)->len > 0), (a)->data[--(a)->len])

/** Set the length to zero. Keeps the allocation for reuse. */
#define arr_clear(a) ((void)((a)->len = 0))

/** Free the elements' memory and reset to empty. Keeps `alloc`, and is safe
 *  to call twice. */
#define arr_free(a)                                                            \
    (arr_free_(&(a)->data, (a)->capacity * sizeof *(a)->data, (a)->alloc),     \
     (a)->len = 0, (void)((a)->capacity = 0))

/** @cond */
[[nodiscard]] bool arr_reserve_(void *data_field, size_t *capacity, size_t len,
                                size_t add, size_t elem,
                                const Allocator *alloc);
void arr_free_(void *data_field, size_t bytes, const Allocator *alloc);
/** @endcond */

/* --- src/rand/rand.h --- */

/**
 * @file
 * @brief Rand: a small, fast, seeded pseudo-random generator.
 *
 *   - splitmix64, so the same seed gives the same sequence on every platform,
 *     unlike rand(). Use it for tests, simulations and shuffles.
 *   - Not for secrets. Use the operating system's generator for keys and
 *     tokens.
 *   - A zero-initialised Rand is valid. Set `state` to pick a seed.
 *
 * @snippet{trimleft} test_rand.c rand
 */


/** Generator state. Copy it to fork a sequence. */
typedef struct Rand {
    uint64_t state; /**< The seed, advanced by every call. */
} Rand;

/** The next 64 random bits. */
[[nodiscard]] uint64_t rand_next(Rand *r);
/** A uniform value in [0, @p n). @p n must not be 0. */
[[nodiscard]] uint64_t rand_below(Rand *r, uint64_t n);

/* --- src/time/time.h --- */

/**
 * @file
 * @brief Time: clocks for measuring how long things take.
 *
 * @snippet{trimleft} test_time.c time
 */


/** Nanoseconds on a monotonic clock. It never goes backwards, but its zero is
 *  arbitrary, so only the difference between two readings means anything. */
[[nodiscard]] uint64_t time_now_ns(void);

/* --- src/log/log.h --- */

/**
 * @file
 * @brief Log: levelled messages for people reading a terminal or a log file.
 *
 *   - Each call writes one line, `LEVEL file:line: message`, with the level
 *     coloured when the output supports it.
 *   - Calls below the runtime level, set with log_set_level(), return before
 *     evaluating their arguments. Calls below #LOG_LEVEL_MIN compile to
 *     nothing.
 *   - Each line goes out in one `fwrite`, so lines from different threads do
 *     not interleave. Set the level and output before starting threads.
 *   - log_fatal() and log_assert() write, flush and abort, whatever the level.
 *
 * @snippet{trimleft} test_log.c log
 *
 * log_color_enabled() follows the conventions other command-line tools use:
 *   - `NO_COLOR` set to anything non-empty turns colour off. It wins over
 *     everything else (https://no-color.org).
 *   - `CLICOLOR_FORCE` or `FORCE_COLOR` set to anything but empty or `0` turns
 *     colour on, even when the output is a pipe or a file.
 *   - Otherwise colour is on when the stream is a terminal and `TERM` is set
 *     and is not `dumb`.
 *
 * @snippet{trimleft} test_log.c color
 */


#include <stdio.h>

/* ANSI escape sequences. Macros, so they join string literals:
 * `LOG_GREEN "ok" LOG_RESET`. Print them only when log_color_enabled(). */
#define LOG_RED "\x1b[31m"    /**< Red text. */
#define LOG_GREEN "\x1b[32m"  /**< Green text. */
#define LOG_YELLOW "\x1b[33m" /**< Yellow text. */
#define LOG_RESET "\x1b[0m"   /**< Back to the terminal's default style. */

/** How much a message matters, from chattiest to most severe. */
typedef enum LogLevel {
    LOG_TRACE, /**< Step-by-step detail, usually off. */
    LOG_DEBUG, /**< Detail for whoever is debugging. */
    LOG_INFO,  /**< Normal progress worth recording. The default level. */
    LOG_WARN,  /**< Something odd that the program survived. */
    LOG_ERROR, /**< An operation failed. */
    LOG_FATAL, /**< The program cannot continue. */
} LogLevel;

#ifndef LOG_LEVEL_MIN
/** Calls below this level compile to nothing. Define it before the first
 *  include of log.h, for example `-DLOG_LEVEL_MIN=LOG_INFO`. */
#define LOG_LEVEL_MIN LOG_TRACE
#endif

/** Drop messages below @p level from now on. */
void log_set_level(LogLevel level);
/** Write messages to @p f from now on. Null means stderr, the default. */
void log_set_output(FILE *f);
/** True when colour escapes written to @p f will render as colour. Reads the
 *  environment on every call, so call it once and keep the answer. */
[[nodiscard]] bool log_color_enabled(FILE *f);

/** Log a printf-style message at LOG_TRACE. */
#define log_trace(...) LOG_AT_(LOG_TRACE, __VA_ARGS__)
/** Log a printf-style message at LOG_DEBUG. */
#define log_debug(...) LOG_AT_(LOG_DEBUG, __VA_ARGS__)
/** Log a printf-style message at LOG_INFO. */
#define log_info(...) LOG_AT_(LOG_INFO, __VA_ARGS__)
/** Log a printf-style message at LOG_WARN. */
#define log_warn(...) LOG_AT_(LOG_WARN, __VA_ARGS__)
/** Log a printf-style message at LOG_ERROR. */
#define log_error(...) LOG_AT_(LOG_ERROR, __VA_ARGS__)
/** Log a printf-style message at LOG_FATAL, flush, and abort. */
#define log_fatal(...) log_fatal_(__FILE__, __LINE__, __VA_ARGS__)
/** Abort through log_fatal() when @p cond is false. Unlike assert(), it stays
 *  in release builds. */
#define log_assert(cond)                                                       \
    ((cond) ? (void)0                                                          \
            : log_fatal_(__FILE__, __LINE__, "assertion failed: %s", #cond))

/** @cond */
extern LogLevel log_level_;

#define LOG_AT_(level, ...)                                                    \
    ((level) >= LOG_LEVEL_MIN && (level) >= log_level_                         \
         ? log_write_((level), __FILE__, __LINE__, __VA_ARGS__)                \
         : (void)0)

void log_write_(LogLevel level, const char *file, int line, const char *fmt,
                ...) __attribute__((format(printf, 4, 5)));
[[noreturn]] void log_fatal_(const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
/** @endcond */

#endif /* MULE_H */

#if defined(MULE_TEST) && !defined(MULE_TEST_DONE)
#define MULE_TEST_DONE

/* --- src/test/test.h --- */

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
 *     Str s = {};
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
static char **test_argv_ = nullptr;
static const char *test_running_ = "";
static bool test_color_ = false; /* set once by test_init */

/* --- Allocator hooks ----------------------------------------------------- */

static long test_live_allocs_ = 0;
static long test_alloc_budget_ = -1; /* allocations left; -1 means no limit */

/** @endcond */

/** Let the next @p n allocations succeed and fail every one after. Pass -1
 *  to stop failing. RUN resets it after each test. */
[[maybe_unused]] static void test_fail_alloc_after(long n) {
    test_alloc_budget_ = n;
}

/** @cond */

static bool test_alloc_allowed_(void) {
    if (test_alloc_budget_ == 0) return false;
    if (test_alloc_budget_ > 0) test_alloc_budget_--;
    return true;
}

[[maybe_unused]] static void *test_malloc_(size_t n) {
    if (!test_alloc_allowed_()) return nullptr;
    void *p = malloc(n);
    if (p != nullptr) test_live_allocs_++;
    return p;
}

[[maybe_unused]] static void *test_calloc_(size_t count, size_t n) {
    if (!test_alloc_allowed_()) return nullptr;
    void *p = calloc(count, n);
    if (p != nullptr) test_live_allocs_++;
    return p;
}

[[maybe_unused]] static void *test_realloc_(void *old, size_t n) {
    if (!test_alloc_allowed_()) return nullptr;
    void *p = realloc(old, n);
    if (p != nullptr && old == nullptr) test_live_allocs_++;
    return p;
}

[[maybe_unused]] static void test_free_(void *p) {
    if (p != nullptr) test_live_allocs_--;
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
    if (p == nullptr) {
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

[[maybe_unused]] static bool test_check_(bool ok, const char *file, int line,
                                         const char *expr) {
    test_checks_++;
    if (!ok) {
        test_fail_at_(file, line);
        printf("CHECK(%s) failed\n", expr);
    }
    return ok;
}

[[maybe_unused]] static bool test_eq_int_(long long a, long long b,
                                          const char *file, int line,
                                          const char *ea, const char *eb) {
    test_checks_++;
    if (a != b) {
        test_fail_at_(file, line);
        printf("%s == %s failed (%lld vs %lld)\n", ea, eb, a, b);
    }
    return a == b;
}

[[maybe_unused]] static bool test_eq_str_(const char *a, const char *b,
                                          const char *file, int line,
                                          const char *ea, const char *eb) {
    bool ok = (a == nullptr || b == nullptr) ? a == b : strcmp(a, b) == 0;
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

[[maybe_unused]] static bool test_eq_mem_(const char *a, size_t alen,
                                          const char *b, size_t blen,
                                          const char *file, int line,
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
[[maybe_unused]] static void test_init(int argc, char **argv) {
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

[[maybe_unused]] static int test_report_(const char *file) {
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


#endif /* MULE_TEST */

#if defined(MULE_IMPLEMENTATION) && !defined(MULE_IMPLEMENTATION_DONE)
#define MULE_IMPLEMENTATION_DONE

/* --- src/mem/mem.c --- */


#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct ArenaBlock {
    ArenaBlock *prev;
    size_t used;
    size_t cap;
    alignas(max_align_t) char data[];
};

void *mem_realloc(const Allocator *a, void *old, size_t old_size,
                  size_t new_size) {
    if (a != nullptr) return a->fn(a->ctx, old, old_size, new_size);
    /* realloc(p, 0) is undefined in C23, so freeing is its own branch. */
    if (new_size == 0) {
        free(old);
        return nullptr;
    }
    return realloc(old, new_size);
}

/* Carve `size` bytes out of `b`, or return null if they do not fit. */
static void *mem_bump_(ArenaBlock *b, size_t size, size_t align) {
    size_t pad = (size_t)-(uintptr_t)(b->data + b->used) & (align - 1);
    size_t start, end;
    if (ckd_add(&start, b->used, pad) ||
        ckd_add(&end, start, size) ||
        end > b->cap)
        return nullptr;
    b->used = end;
    return b->data + start;
}

void *arena_alloc(Arena *a, size_t size, size_t align) {
    assert(align != 0 && (align & (align - 1)) == 0);
    void *p = a->head != nullptr ? mem_bump_(a->head, size, align) : nullptr;
    if (p == nullptr) {
        /* Room for the worst-case padding too, so the bump cannot fail. */
        size_t cap, bytes;
        if (ckd_add(&cap, size, align - 1)) return nullptr;
        if (cap < MEM_ARENA_BLOCK) cap = MEM_ARENA_BLOCK;
        if (ckd_add(&bytes, cap, sizeof(ArenaBlock))) return nullptr;

        ArenaBlock *b = malloc(bytes);
        if (b == nullptr) return nullptr;
        *b = (ArenaBlock){ .prev = a->head, .cap = cap };
        a->head = b;
        p = mem_bump_(b, size, align);
    }
    a->last = p;
    return p;
}

static void *mem_arena_fn_(void *ctx, void *old, size_t old_size,
                           size_t new_size) {
    Arena *a = ctx;

    /* The newest allocation ends at head->used, so it can move that mark. */
    if (old != nullptr && old == a->last) {
        ArenaBlock *b = a->head;
        size_t start = (size_t)((char *)old - b->data);
        size_t end;
        if (new_size == 0) {
            b->used = start;
            a->last = nullptr;
            return nullptr;
        }
        if (!ckd_add(&end, start, new_size) && end <= b->cap) {
            b->used = end;
            return old;
        }
    }

    if (new_size == 0) return nullptr; /* older allocations wait for reset */
    void *p = arena_alloc(a, new_size, alignof(max_align_t));
    if (p != nullptr && old != nullptr)
        memcpy(p, old, old_size < new_size ? old_size : new_size);
    return p;
}

const Allocator *arena_allocator(Arena *a) {
    a->allocator = (Allocator){ .fn = mem_arena_fn_, .ctx = a };
    return &a->allocator;
}

void arena_reset(Arena *a) {
    if (a->head == nullptr) return;
    for (ArenaBlock *b = a->head->prev; b != nullptr;) {
        ArenaBlock *prev = b->prev;
        free(b);
        b = prev;
    }
    a->head->prev = nullptr;
    a->head->used = 0;
    a->last = nullptr;
}

void arena_free(Arena *a) {
    for (ArenaBlock *b = a->head; b != nullptr;) {
        ArenaBlock *prev = b->prev;
        free(b);
        b = prev;
    }
    /* Keep the allocator, so Strs that point at it can still allocate. */
    *a = (Arena){ .allocator = a->allocator };
}

/* --- src/str/str.c --- */


#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Grow so that `needed` content bytes fit. Amortised doubling. */
static bool str_grow_(Str *s, size_t needed) {
    if (needed <= s->capacity && s->data != nullptr) return true;

    size_t cap = s->capacity ? s->capacity : 15;
    while (cap < needed) {
        if (ckd_mul(&cap, cap, 2)) {
            cap = needed;
            break;
        }
    }

    size_t bytes;
    if (ckd_add(&bytes, cap, 1)) return false;

    size_t old = s->data != nullptr ? s->capacity + 1 : 0;
    char *p = mem_realloc(s->alloc, s->data, old, bytes);
    if (p == nullptr) return false;
    if (s->data == nullptr) p[0] = '\0';

    s->data = p;
    s->capacity = cap;
    return true;
}

bool str_with_capacity(Str *s, size_t capacity) {
    *s = (Str){ .alloc = s->alloc };
    return str_grow_(s, capacity);
}

bool str_from(Str *s, StrView v) {
    *s = (Str){ .alloc = s->alloc };
    return str_append(s, v);
}

bool str_from_cstr(Str *s, const char *cstr) {
    return str_from(s, sv_from_cstr(cstr));
}

void str_free(Str *s) {
    if (s->data != nullptr)
        (void)mem_realloc(s->alloc, s->data, s->capacity + 1, 0);
    *s = (Str){ .alloc = s->alloc };
}

bool str_reserve(Str *s, size_t additional) {
    size_t needed;
    if (ckd_add(&needed, s->len, additional)) return false;
    return str_grow_(s, needed);
}

bool str_shrink_to_fit(Str *s) {
    if (s->data == nullptr || s->capacity == s->len) return true;
    char *p = mem_realloc(s->alloc, s->data, s->capacity + 1, s->len + 1);
    if (p == nullptr) return false;
    s->data = p;
    s->capacity = s->len;
    return true;
}

void str_clear(Str *s) {
    s->len = 0;
    if (s->data != nullptr) s->data[0] = '\0';
}

bool str_push(Str *s, char c) {
    /* Locals, because a char store may alias `*s` and force reloads. */
    char *data = s->data;
    size_t len = s->len;
    if (data != nullptr && len < s->capacity) {
        data[len] = c;
        data[len + 1] = '\0';
        s->len = len + 1;
        return true;
    }
    return str_append(s, (StrView){ .data = &c, .len = 1 });
}

bool str_append(Str *s, StrView v) {
    if (v.len == 0) return true;

    /* Fast path: it fits, so the buffer cannot move under `v`. */
    if (s->data != nullptr && v.len <= s->capacity - s->len) {
        memmove(s->data + s->len, v.data, v.len);
        s->len += v.len;
        s->data[s->len] = '\0';
        return true;
    }

    /* `v` may point into our own buffer (e.g. str_append(&s, str_view(&s))).
     * Remember its offset, since growing may move the allocation. */
    const bool aliased = s->data != nullptr &&
                         v.data >= s->data &&
                         v.data <= s->data + s->len;
    const size_t off = aliased ? (size_t)(v.data - s->data) : 0;

    if (!str_reserve(s, v.len)) return false;

    const char *src = aliased ? s->data + off : v.data;
    memmove(s->data + s->len, src, v.len);
    s->len += v.len;
    s->data[s->len] = '\0';
    return true;
}

bool str_append_cstr(Str *s, const char *cstr) {
    return str_append(s, sv_from_cstr(cstr));
}

bool str_appendf(Str *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    bool ok = str_vappendf(s, fmt, ap);
    va_end(ap);
    return ok;
}

bool str_vappendf(Str *s, const char *fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);

    /* Format straight into the spare capacity; most calls fit and stop here. */
    size_t room = s->data != nullptr ? s->capacity - s->len + 1 : 0;
    char *end = room != 0 ? s->data + s->len : nullptr;
    int n = vsnprintf(end, room, fmt, ap);
    bool ok = n >= 0 && (size_t)n < room;
    if (ok) {
        s->len += (size_t)n;
    } else {
        /* The attempt overwrote the terminator; restore it before failing. */
        if (end != nullptr) *end = '\0';
        if (n >= 0 && str_reserve(s, (size_t)n)) {
            vsnprintf(s->data + s->len, (size_t)n + 1, fmt, ap2);
            s->len += (size_t)n;
            ok = true;
        }
    }

    va_end(ap2);
    return ok;
}

bool str_append_file(Str *s, FILE *f) {
    /* Read into a scratch Str so a failure part-way leaves `s` untouched. */
    Str tmp = { .alloc = s->alloc };
    bool ok = true;
    for (;;) {
        if (!str_reserve(&tmp, 4096)) {
            ok = false;
            break;
        }
        size_t room = tmp.capacity - tmp.len;
        size_t n = fread(tmp.data + tmp.len, 1, room, f);
        tmp.len += n;
        if (n < room) {
            ok = !ferror(f);
            break;
        }
    }

    if (ok && s->data == nullptr) {
        /* `s` owns nothing, so take the buffer instead of copying it. */
        tmp.data[tmp.len] = '\0';
        *s = tmp;
        return true;
    }
    ok = ok && str_append(s, str_view(&tmp));
    str_free(&tmp);
    return ok;
}

bool str_read_line(Str *s, FILE *f) {
    /* Read after the current contents, so failure can restore them. */
    size_t old = s->len;
    bool got = false, ok = true;
    flockfile(f);
    for (int c; (c = getc_unlocked(f)) != EOF;) {
        got = true;
        if (c == '\n') break;
        if (!str_push(s, (char)c)) {
            ok = false;
            break;
        }
    }
    funlockfile(f);

    if (!ok || !got || ferror(f)) {
        s->len = old;
        if (s->data != nullptr) s->data[old] = '\0';
        return false;
    }
    if (s->data == nullptr) return true; /* an empty first line */
    memmove(s->data, s->data + old, s->len - old);
    s->len -= old;
    s->data[s->len] = '\0';
    return true;
}

StrView str_view(const Str *s) {
    return (StrView){ .data = str_cstr(s), .len = s->len };
}

const char *str_cstr(const Str *s) {
    return s->data != nullptr ? s->data : "";
}

bool str_eq(StrView a, StrView b) {
    return a.len == b.len && (a.len == 0 || memcmp(a.data, b.data, a.len) == 0);
}

StrView sv_from_cstr(const char *cstr) {
    if (cstr == nullptr) return (StrView){ .data = "", .len = 0 };
    return (StrView){ .data = cstr, .len = strlen(cstr) };
}

/* --- Views --------------------------------------------------------------- */

size_t sv_find_byte(StrView v, char c) {
    if (v.len == 0) return SV_NPOS;
    const char *p = memchr(v.data, c, v.len);
    return p != nullptr ? (size_t)(p - v.data) : SV_NPOS;
}

size_t sv_find(StrView v, StrView needle) {
    if (needle.len == 0) return 0;
    if (needle.len > v.len) return SV_NPOS;

    /* memchr to each candidate first byte, then compare the rest. */
    size_t last = v.len - needle.len;
    for (size_t i = 0; i <= last; i++) {
        const char *p = memchr(v.data + i, needle.data[0], last - i + 1);
        if (p == nullptr) return SV_NPOS;
        i = (size_t)(p - v.data);
        if (memcmp(p + 1, needle.data + 1, needle.len - 1) == 0) return i;
    }
    return SV_NPOS;
}

int sv_cmp(StrView a, StrView b) {
    size_t n = a.len < b.len ? a.len : b.len;
    int c = n != 0 ? memcmp(a.data, b.data, n) : 0;
    if (c != 0) return c;
    return (a.len > b.len) - (a.len < b.len);
}

bool sv_starts_with(StrView v, StrView prefix) {
    return prefix.len <= v.len && str_eq(sv_slice(v, 0, prefix.len), prefix);
}

bool sv_ends_with(StrView v, StrView suffix) {
    return suffix.len <= v.len &&
           str_eq(sv_slice(v, v.len - suffix.len, v.len), suffix);
}

StrView sv_slice(StrView v, size_t from, size_t to) {
    assert(from <= to && to <= v.len);
    /* An empty view may hold a null pointer, and null + 0 is undefined. */
    return (StrView){ .data = v.len != 0 ? v.data + from : v.data,
                      .len = to - from };
}

static bool sv_space_(char c) {
    return c == ' ' || (c >= '\t' && c <= '\r');
}

StrView sv_trim(StrView v) {
    size_t from = 0, to = v.len;
    while (from < to && sv_space_(v.data[from])) from++;
    while (to > from && sv_space_(v.data[to - 1])) to--;
    return sv_slice(v, from, to);
}

bool sv_split_next(StrView *rest, StrView sep, StrView *piece) {
    assert(sep.len != 0);
    if (rest->len == 0) return false;

    size_t at = sv_find(*rest, sep);
    if (at == SV_NPOS) {
        *piece = *rest;
        *rest = sv_slice(*rest, rest->len, rest->len);
    } else {
        *piece = sv_slice(*rest, 0, at);
        *rest = sv_slice(*rest, at + sep.len, rest->len);
    }
    return true;
}

static bool sv_in_set_(const uint64_t set[4], char c) {
    unsigned char u = (unsigned char)c;
    return (set[u >> 6] >> (u & 63)) & 1;
}

bool sv_token_next(StrView *rest, StrView delims, StrView *token) {
    /* One bit per byte value, like strspn builds, so each test is O(1). */
    uint64_t set[4] = {};
    for (size_t i = 0; i < delims.len; i++) {
        unsigned char c = (unsigned char)delims.data[i];
        set[c >> 6] |= UINT64_C(1) << (c & 63);
    }

    size_t from = 0, n = rest->len;
    while (from < n && sv_in_set_(set, rest->data[from])) from++;
    size_t to = from;
    while (to < n && !sv_in_set_(set, rest->data[to])) to++;

    if (from == to) {
        *rest = sv_slice(*rest, n, n);
        return false;
    }
    *token = sv_slice(*rest, from, to);
    *rest = sv_slice(*rest, to, n);
    return true;
}

/* --- Parsing ------------------------------------------------------------- */

bool sv_parse_i64(StrView v, int64_t *out) {
    size_t i = 0;
    bool neg = false;
    if (v.len > 0 && (v.data[0] == '-' || v.data[0] == '+')) {
        neg = v.data[0] == '-';
        i = 1;
    }
    if (i == v.len) return false;

    uint64_t mag = 0;
    for (; i < v.len; i++) {
        unsigned d = (unsigned)(unsigned char)v.data[i] - '0';
        if (d > 9 || ckd_mul(&mag, mag, 10) || ckd_add(&mag, mag, d))
            return false;
    }
    if (mag > (uint64_t)INT64_MAX + neg) return false;
    /* -(mag - 1) - 1 reaches INT64_MIN without overflowing on the way. */
    *out = neg ? -(int64_t)(mag - 1) - 1 : (int64_t)mag;
    return true;
}

bool sv_parse_f64(StrView v, double *out) {
    char buf[256];
    if (v.len == 0 || v.len >= sizeof buf || sv_space_(v.data[0])) return false;
    memcpy(buf, v.data, v.len);
    buf[v.len] = '\0';

    char *end;
    errno = 0;
    double d = strtod(buf, &end);
    if (end != buf + v.len || (errno == ERANGE && isinf(d))) return false;
    *out = d;
    return true;
}

/* --- src/arr/arr.c --- */


#include <string.h>

/* `data_field` is the address of the array's `T *data`. Copying the pointer
 * through memcpy reads and writes it without aliasing it as a `void *`. */
bool arr_reserve_(void *data_field, size_t *capacity, size_t len, size_t add,
                  size_t elem, const Allocator *alloc) {
    size_t needed;
    if (ckd_add(&needed, len, add)) return false;
    if (needed <= *capacity) return true;

    size_t cap = *capacity != 0 ? *capacity : 8;
    while (cap < needed) {
        if (ckd_mul(&cap, cap, 2)) {
            cap = needed;
            break;
        }
    }
    size_t bytes;
    if (ckd_mul(&bytes, cap, elem)) return false;

    void *old;
    memcpy(&old, data_field, sizeof old);
    void *p = mem_realloc(alloc, old, *capacity * elem, bytes);
    if (p == nullptr) return false;
    memcpy(data_field, &p, sizeof p);
    *capacity = cap;
    return true;
}

void arr_free_(void *data_field, size_t bytes, const Allocator *alloc) {
    void *p;
    memcpy(&p, data_field, sizeof p);
    if (p != nullptr) (void)mem_realloc(alloc, p, bytes, 0);
    p = nullptr;
    memcpy(data_field, &p, sizeof p);
}

/* --- src/rand/rand.c --- */


#include <assert.h>

uint64_t rand_next(Rand *r) {
    uint64_t z = (r->state += 0x9E3779B97F4A7C15u);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
    return z ^ (z >> 31);
}

uint64_t rand_below(Rand *r, uint64_t n) {
    assert(n != 0);
    /* Reject the low values that would make some results more likely. */
    uint64_t threshold = -n % n;
    for (;;) {
        uint64_t x = rand_next(r);
        if (x >= threshold) return x % n;
    }
}

/* --- src/time/time.c --- */


#include <time.h>

uint64_t time_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

/* --- src/log/log.c --- */


#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

LogLevel log_level_ = LOG_INFO;
static FILE *log_out_;      /* null means stderr */
static int log_color_ = -1; /* -1 until the first message decides */

static const char *const log_names_[] = {
    [LOG_TRACE] = "TRACE", [LOG_DEBUG] = "DEBUG", [LOG_INFO] = "INFO ",
    [LOG_WARN] = "WARN ",  [LOG_ERROR] = "ERROR", [LOG_FATAL] = "FATAL",
};
static const char *const log_colors_[] = {
    [LOG_TRACE] = "",        [LOG_DEBUG] = "",      [LOG_INFO] = LOG_GREEN,
    [LOG_WARN] = LOG_YELLOW, [LOG_ERROR] = LOG_RED, [LOG_FATAL] = LOG_RED,
};

void log_set_level(LogLevel level) {
    log_level_ = level;
}

void log_set_output(FILE *f) {
    log_out_ = f;
    log_color_ = -1;
}

/* Set to anything other than empty or "0". */
static bool log_env_on_(const char *name) {
    const char *v = getenv(name);
    return v != nullptr && v[0] != '\0' && strcmp(v, "0") != 0;
}

bool log_color_enabled(FILE *f) {
    const char *no_color = getenv("NO_COLOR");
    if (no_color != nullptr && no_color[0] != '\0') return false;
    if (log_env_on_("CLICOLOR_FORCE") || log_env_on_("FORCE_COLOR"))
        return true;
    if (!isatty(fileno(f))) return false;

    const char *term = getenv("TERM");
    return term != nullptr && term[0] != '\0' && strcmp(term, "dumb") != 0;
}

__attribute__((format(printf, 4, 0))) static FILE *
log_vwrite_(LogLevel level, const char *file, int line, const char *fmt,
            va_list ap) {
    FILE *out = log_out_ != nullptr ? log_out_ : stderr;
    if (log_color_ < 0) log_color_ = log_color_enabled(out);
    const char *on = log_color_ ? log_colors_[level] : "";
    const char *off = on[0] != '\0' ? LOG_RESET : "";

    /* shortcut: one allocation per line. Reuse a thread_local buffer if
     * logging ever shows up in a profile. */
    Str s = {};
    bool ok = str_appendf(&s, "%s%s%s %s:%d: ", on, log_names_[level], off,
                          file, line) &&
              str_vappendf(&s, fmt, ap) &&
              str_push(&s, '\n');
    if (ok)
        fwrite(s.data, 1, s.len, out);
    else
        fprintf(out, "%s %s:%d: (out of memory formatting this message)\n",
                log_names_[level], file, line);
    str_free(&s);
    return out;
}

void log_write_(LogLevel level, const char *file, int line, const char *fmt,
                ...) {
    va_list ap;
    va_start(ap, fmt);
    log_vwrite_(level, file, line, fmt, ap);
    va_end(ap);
}

void log_fatal_(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    FILE *out = log_vwrite_(LOG_FATAL, file, line, fmt, ap);
    va_end(ap);
    fflush(out);
    abort();
}

#endif /* MULE_IMPLEMENTATION */
