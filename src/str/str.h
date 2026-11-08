#pragma once
/**
 * @defgroup mod_str Str and StrView
 * @brief Owned, growable byte strings, and borrowed views into them.
 *
 * Two types do all the work. A Str owns a heap buffer and can grow. A
 * StrView borrows bytes that something else owns and never allocates. The
 * working pattern is to put text into a Str once, then pass StrViews around
 * to search it, split it and cut it. Copy a view into a new Str only when
 * the bytes must outlive their owner.
 *
 * @snippet{trimleft} test_str.c build
 *
 * ## Str
 *
 *   - `data` is NUL-terminated whenever it is non-null, so str_cstr() can
 *     hand it to C APIs. `len` is still the authority, and embedded NULs are
 *     allowed.
 *   - `capacity` counts content bytes. The allocation is `capacity + 1`
 *     bytes, to hold the terminator.
 *   - A zero-initialised Str (`Str s = { 0 };`) is a valid empty string
 *     that owns no memory. Every function accepts it.
 *   - The first allocation holds 15 bytes, and each growth doubles the
 *     capacity, so appending one byte at a time costs O(1) amortised.
 *   - `alloc` says where the bytes live, and null means the libc heap. Set
 *     it before the first allocation and leave it alone after. str_free()
 *     keeps it.
 *   - Pass a Str to functions as `Str *`. `Str b = a;` copies the pointer,
 *     not the bytes, and freeing both is a double free.
 *
 * @snippet{trimleft} test_str.c arena_str
 *
 * ## StrView
 *
 *   - A view is a pointer and a length. Pass it by value.
 *   - It is valid while its owner is alive and has not moved its buffer.
 *     Any call that may grow a Str may move it, and every view into it then
 *     dangles.
 *   - It is not NUL-terminated in general. Print it with `%.*s` and
 *     STR_FMT, never with `%s`.
 *   - No StrView function allocates, so none of them can fail. Make a view
 *     with SV(), sv_from_cstr(), str_view() or sv_slice().
 *
 * ## Errors
 *
 * A function that may allocate returns `bool`. False means out of memory, a
 * size that would overflow, or a read error for the file functions. A call
 * that returns false leaves the Str exactly as it was, so `&&` can chain
 * calls and stops at the first failure.
 *
 * The tutorial [Str and StrView in practice](str-and-strview.html) walks
 * through all of this with programs you can run, and shows what the common
 * mistakes look like under AddressSanitizer.
 * @{
 */

#include "base/base.h"
#include "mem/mem.h"

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

/** View of a string literal, with its length computed at compile time. Only
 *  a literal compiles, so a `char *` cannot silently get the length of a
 *  pointer. Use sv_from_cstr() for pointers. */
#define SV(lit) ((StrView){ .data = "" lit "", .len = sizeof(lit) - 1 })
/** printf helper: `printf("%.*s", STR_FMT(s))` prints exactly len bytes.
 *  Works on a Str and on a StrView.
 *  @par Example
 *  @snippet{trimleft} test_str.c fmt */
#define STR_FMT(s) (int)(s).len, (s).data

/** @name Lifetime
 *  Initialise a Str with contents, and free it.
 *  @{ */

/** Initialise @p s as empty with room for @p capacity bytes of content.
 *  @param[out] s  Overwritten without being freed. Only `alloc` is kept.
 *  @param capacity  Content bytes to make room for.
 *  @return False when out of memory.
 *  @warning It does not free what @p s held. Call str_free() first on a Str
 *  that owns memory. */
MULE_NODISCARD bool str_with_capacity(Str *s, size_t capacity);
/** Initialise @p s as a copy of @p v.
 *  @param[out] s  Overwritten without being freed. Only `alloc` is kept.
 *  @param v  The bytes to copy.
 *  @return False when out of memory.
 *  @warning It does not free what @p s held. Call str_free() first on a Str
 *  that owns memory. */
MULE_NODISCARD bool str_from(Str *s, StrView v);
/** Initialise @p s as a copy of @p cstr. Null gives an empty string.
 *  @return False when out of memory.
 *  @see str_from() */
MULE_NODISCARD bool str_from_cstr(Str *s, const char *cstr);
/** Free the buffer and reset @p s to the zero state. Keeps `alloc`, so the
 *  string can be reused. Safe to call twice.
 *  @par Example
 *  @snippet{trimleft} test_str.c free */
void str_free(Str *s);

/** @} */

/** @name Capacity
 *  Control allocation when you know more than the doubling policy does.
 *  @{ */

/** Ensure @p additional more bytes fit without another allocation. Reserve
 *  before taking views, and appends within the reserve will not move them.
 *  @return False when out of memory or `len + additional` overflows. */
MULE_NODISCARD bool str_reserve(Str *s, size_t additional);
/** Shrink the allocation to exactly `len + 1` bytes.
 *  @return False when the allocator fails. @p s is then unchanged. */
MULE_NODISCARD bool str_shrink_to_fit(Str *s);
/** Set the length to zero. Keeps the allocation for reuse, so a loop that
 *  builds one string per pass allocates once.
 *  @par Example
 *  @snippet{trimleft} test_str.c clear */
void str_clear(Str *s);

/** @} */

/** @name Appending
 *  Add bytes to the end. Each call may move `data`.
 *  @{ */

/** Append the single byte @p c.
 *  @return False when out of memory. @p s is then unchanged. */
MULE_NODISCARD bool str_push(Str *s, char c);
/** Append @p v. @p v may point into @p s itself.
 *  @return False when out of memory. @p s is then unchanged.
 *  @par Example
 *  @snippet{trimleft} test_str.c self_append */
MULE_NODISCARD bool str_append(Str *s, StrView v);
/** Append @p cstr. A null @p cstr appends nothing.
 *  @return False when out of memory. @p s is then unchanged. */
MULE_NODISCARD bool str_append_cstr(Str *s, const char *cstr);
/** Append the printf-style expansion of @p fmt. The compiler checks the
 *  arguments against the format.
 *  @return False when out of memory or the format fails. @p s is then
 *  unchanged.
 *  @par Example
 *  @snippet{trimleft} test_str.c appendf */
MULE_NODISCARD bool str_appendf(Str *s, const char *fmt, ...) MULE_PRINTF(2, 3);
/** str_appendf() for a `va_list`, like vprintf.
 *  @param s  The string to append to.
 *  @param fmt  A printf format for the arguments in @p ap.
 *  @param ap  Used up afterwards. va_copy it first to use it again.
 *  @return False when out of memory or the format fails. */
MULE_NODISCARD bool str_vappendf(Str *s, const char *fmt, va_list ap)
    MULE_PRINTF(2, 0);

/** @} */

/** @name Reading files
 *  Fill a Str from a `FILE *`.
 *  @{ */

/** Append everything left to read from @p f. When @p s owns no buffer yet,
 *  it takes the read buffer over instead of copying it.
 *  @return False on a read error or out of memory. @p s is then unchanged.
 *  Check ferror(f) before fclose to tell the two apart.
 *  @par Example
 *  @snippet{trimleft} test_str.c append_file */
MULE_NODISCARD bool str_append_file(Str *s, FILE *f);
/** Replace the contents of @p s with the next line of @p f, without its
 *  `\n`. A `\r` before it is kept, and embedded NULs are allowed.
 *  @return False at end of input, on a read error (check ferror(f)) or out
 *  of memory. The contents of @p s are then unchanged, though its capacity
 *  may have grown.
 *  @par Example
 *  @snippet{trimleft} test_str.c read_line */
MULE_NODISCARD bool str_read_line(Str *s, FILE *f);

/** @} */

/** @name Access and comparison
 *  Look at a Str, or turn a C string into a view.
 *  @{ */

/** Borrow the whole of @p s.
 *  @warning Invalidated by any call that may reallocate @p s. */
MULE_NODISCARD StrView str_view(const Str *s);
/** NUL-terminated contents of @p s, for C APIs such as fopen and puts.
 *  Never null, even for an empty Str.
 *  @warning Invalidated by any call that may reallocate @p s. */
MULE_NODISCARD const char *str_cstr(const Str *s);
/** True when @p a and @p b hold the same bytes.
 *  @par Example
 *  @snippet{trimleft} test_str.c eq
 *  @see sv_cmp() to order strings. */
MULE_NODISCARD bool str_eq(StrView a, StrView b);
/** View of @p cstr, measured with strlen. Null gives an empty view. Use it
 *  for `argv[i]`, getenv and other C strings. */
MULE_NODISCARD StrView sv_from_cstr(const char *cstr);

/** @} */

/** @name Searching and slicing
 *  Inspect a view and cut pieces out of it. Nothing here allocates.
 *  @{ */

/** Returned by the find functions when there is no match. */
#define SV_NPOS SIZE_MAX

/** Index of the first @p c in @p v.
 *  @return The index, or #SV_NPOS when @p c does not occur. */
MULE_NODISCARD size_t sv_find_byte(StrView v, char c);
/** Index of the first @p needle in @p v. An empty needle is at 0.
 *  @return The index, or #SV_NPOS when @p needle does not occur.
 *  @par Example
 *  @snippet{trimleft} test_str.c find */
MULE_NODISCARD size_t sv_find(StrView v, StrView needle);
/** Byte order like memcmp. A prefix sorts before the longer string, so it
 *  works as a qsort comparison on views.
 *  @return Negative, zero or positive as @p a sorts before, with or after
 *  @p b. */
MULE_NODISCARD int sv_cmp(StrView a, StrView b);
/** True when @p v begins with @p prefix. */
MULE_NODISCARD bool sv_starts_with(StrView v, StrView prefix);
/** True when @p v ends with @p suffix. */
MULE_NODISCARD bool sv_ends_with(StrView v, StrView suffix);
/** Bytes @p from up to but not including @p to.
 *  @pre `from <= to && to <= v.len`. An assert checks it. */
MULE_NODISCARD StrView sv_slice(StrView v, size_t from, size_t to);
/** @p v without leading and trailing ASCII whitespace: space, `\t`, `\n`,
 *  `\v`, `\f` and `\r`. */
MULE_NODISCARD StrView sv_trim(StrView v);

/** @} */

/** @name Splitting
 *  Walk a view piece by piece. Both functions cut the next piece off the
 *  front of `rest`, and return false when nothing is left.
 *  @{ */

/** Cut the piece before @p sep off @p rest. Empty pieces are kept, so
 *  `a,,b,` splits as `a`, empty, `b`. Use it for fields and lines.
 *  @param[in,out] rest  The text still to split. It shrinks on every call.
 *  @param sep  The separator. It may be several bytes, such as `"\r\n"`.
 *  @param[out] piece  The next piece. Unchanged when the call returns false.
 *  @return False when @p rest is empty.
 *  @pre @p sep is not empty.
 *  @par Example
 *  @snippet{trimleft} test_str.c split
 *  @see sv_token_next() to skip empty pieces. */
MULE_NODISCARD bool sv_split_next(StrView *rest, StrView sep, StrView *piece);
/** strtok without the mutation: skip @p delims, then cut the next run of
 *  other bytes. Empty pieces never appear. Use it for words.
 *  @param[in,out] rest  The text still to split. It shrinks on every call.
 *  @param delims  A set of single bytes, any of which separates tokens.
 *  @param[out] token  The next token. Unchanged when the call returns false.
 *  @return False when only delimiters are left.
 *  @par Example
 *  @snippet{trimleft} test_str.c token
 *  @see sv_split_next() to keep empty pieces. */
MULE_NODISCARD bool sv_token_next(StrView *rest, StrView delims,
                                  StrView *token);

/** @} */

/** @name Parsing
 *  Turn a whole view into a number. Partial matches fail.
 *  @{ */

/** Parse all of @p v as a base-10 integer with an optional sign.
 *  @param v  The whole text to parse.
 *  @param[out] out  The value. Unchanged when the call returns false.
 *  @return False on whitespace, trailing bytes or overflow.
 *  @par Example
 *  @snippet{trimleft} test_str.c parse */
MULE_NODISCARD bool sv_parse_i64(StrView v, int64_t *out);
/** Parse all of @p v as a double, in any form strtod accepts in the C
 *  locale.
 *  @param v  The whole text to parse.
 *  @param[out] out  The value. Unchanged when the call returns false.
 *  @return False on whitespace, trailing bytes, overflow to infinity, or a
 *  view over 255 bytes. */
MULE_NODISCARD bool sv_parse_f64(StrView v, double *out);

/** @} */

/** @} */
