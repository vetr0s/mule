#pragma once
/**
 * @file
 * @brief Str: an owned, growable, heap-allocated byte string.
 *
 *   - `data` is always NUL-terminated when non-null, so it can be passed to
 *     C APIs, but `len` is authoritative (embedded NULs are allowed).
 *   - `capacity` is the number of bytes usable for content, excluding the
 *     terminator (the allocation is `capacity + 1` bytes).
 *   - A zero-initialised Str (`Str s = {};`) is a valid empty string and owns
 *     no memory; every function accepts it.
 *   - Functions that may allocate return `bool` (false = out of memory, size
 *     overflow, or a read error). On failure the string is left unchanged.
 *
 * Examples on this page are excerpts of tests/test_str.c and run with
 * `make test`.
 *
 * @snippet{trimleft} test_str.c build
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/** Owned, growable byte string. Zero-initialise before first use. */
typedef struct Str {
    char *data;      /**< Heap buffer, or null when nothing is allocated. */
    size_t len;      /**< Bytes of content, excluding the terminator. */
    size_t capacity; /**< Content bytes that fit before the next realloc. */
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
/** Append everything left to read from @p f. On false, check ferror(f).
 *  @snippet{trimleft} test_str.c append_file */
[[nodiscard]] bool str_append_file(Str *s, FILE *f);

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
