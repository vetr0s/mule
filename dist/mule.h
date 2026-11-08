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
 * @defgroup mod_base Base
 * @brief What every module needs, included before anything else.
 *
 * Every header includes base.h first, so a program that includes any
 * module gets all of this:
 *   - `_POSIX_C_SOURCE` set to 200809L, unless the program set it first.
 *     It only takes effect before the first system header, so include MULE
 *     before any of them.
 *   - `<stdbool.h>`, `<stddef.h>` and `<stdint.h>`.
 *   - `ckd_add`, `ckd_sub` and `ckd_mul` for overflow-checked arithmetic.
 *     They come from C23's `<stdckdint.h>` when the compiler has it, and
 *     from the gcc and clang builtins otherwise.
 *   - The `MULE_NODISCARD`, `MULE_UNUSED`, `MULE_NORETURN` and
 *     `MULE_PRINTF` attribute macros. They are left out of the signatures
 *     in this reference.
 *
 * ## Layout rules
 *
 * These are for changing the library, not using it.
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
 * Targets are Unix with clang 17+ or gcc 13+. The floor is C11, written as
 * C99. Attributes go through the MULE_ macros below, so a compiler without
 * GNU attributes still builds the library.
 * @{
 */

/** @cond */
/* Feature macros only take effect before the first system header. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* gcc ignores a (void) cast on warn_unused_result, so discard by assigning. */
#ifdef __GNUC__
#define MULE_NODISCARD __attribute__((warn_unused_result))
#define MULE_UNUSED __attribute__((unused))
#define MULE_NORETURN __attribute__((noreturn))
#define MULE_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define MULE_NODISCARD
#define MULE_UNUSED
#define MULE_NORETURN _Noreturn
#define MULE_PRINTF(fmt, args)
#endif

#if __has_include(<stdckdint.h>)
#include <stdckdint.h>
#else
/* gcc 13 has the builtins but not the C23 header. */
#define ckd_add(r, a, b) __builtin_add_overflow((a), (b), (r))
#define ckd_sub(r, a, b) __builtin_sub_overflow((a), (b), (r))
#define ckd_mul(r, a, b) __builtin_mul_overflow((a), (b), (r))
#endif
/** @endcond */

/** @name Version
 *  The library's version, which follows Semantic Versioning. See
 *  CHANGELOG.md for what changed in each one.
 *  @{ */
/** Bumped for breaking changes, once the version reaches 1.0.0. */
#define MULE_VERSION_MAJOR 0
/** Bumped for new features. Before 1.0.0, also for breaking changes. */
#define MULE_VERSION_MINOR 0
/** Bumped for fixes that change no behaviour a program relies on. */
#define MULE_VERSION_PATCH 1
/** All three numbers as a string, such as "0.0.1". */
#define MULE_VERSION "0.0.1"
/** @} */

/** @} */

/* --- src/mem/mem.h --- */

/**
 * @defgroup mod_mem Mem
 * @brief The allocator interface, and an arena that frees everything at once.
 *
 * ## Allocators
 *
 * An Allocator is one realloc-shaped function and a context pointer. Every
 * module that allocates takes a `const Allocator *`, and null means the libc
 * heap. A Str or an Arr keeps the pointer in its `alloc` field, so the
 * caller picks where its bytes live once and the rest of the code does not
 * care.
 *
 * All allocation in the library goes through mem_realloc(). Only this module
 * calls malloc, realloc and free.
 *
 * ## Arenas
 *
 * An Arena hands out memory from large blocks and frees all of it in one
 * call. Use one for data that lives and dies together, such as everything
 * built while handling one request or parsing one file.
 *
 *   - A zero-initialised Arena (`Arena a = { 0 };`) is valid and owns nothing.
 *     The first allocation creates a block of at least #MEM_ARENA_BLOCK
 *     bytes.
 *   - arena_alloc() hands out raw aligned bytes. arena_allocator() wraps the
 *     arena in an Allocator, for a Str or an Arr to grow inside.
 *   - Growing the most recent allocation happens in place when the block has
 *     room, so a Str growing in an arena does not leave a copy behind at
 *     every doubling. Freeing the most recent allocation gives its bytes
 *     back. Older allocations stay until arena_reset() or arena_free().
 *
 * @snippet{trimleft} test_mem.c arena
 *
 * @warning An Arena must stay where it is once arena_allocator() has been
 * called. The Allocator's context points at it, so copying or moving the
 * Arena leaves every Str that uses it pointing at the old address.
 * @{
 */


/* Callers pass alignof(T) to arena_alloc. */
#include <stdalign.h>

/** @name Allocators
 *  The interface every allocating module takes.
 *  @{ */

/** Allocate, resize or free, depending on @p old and @p new_size. It has
 *  the same contract as mem_realloc(), with @p ctx from the Allocator. */
typedef void *AllocatorFn(void *ctx, void *old, size_t old_size,
                          size_t new_size);

/** Where a module gets its memory. */
typedef struct Allocator {
    AllocatorFn *fn; /**< Called with `ctx` and the mem_realloc arguments. */
    void *ctx;       /**< Passed to `fn` unchanged. */
} Allocator;

/** Resize @p old from @p old_size to @p new_size bytes through @p a.
 *  @param a  The allocator, or null for the libc heap.
 *  @param old  The block to resize, or null to allocate a new one.
 *  @param old_size  The size @p old was allocated with. Ignored when @p old
 *  is null.
 *  @param new_size  The size wanted. Zero frees @p old.
 *  @return The new block, or null on failure with @p old untouched. A free
 *  returns null too, so `p = mem_realloc(a, p, n, 0);` frees and clears. */
MULE_NODISCARD void *mem_realloc(const Allocator *a, void *old, size_t old_size,
                                 size_t new_size);

/** @} */

/** @name Arenas
 *  Bump allocation from blocks that are freed together.
 *  @{ */

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

/** Hand out @p size bytes from @p a.
 *  @param a  The arena.
 *  @param size  Bytes wanted.
 *  @param align  A power of two, usually `alignof(T)`. An assert checks it.
 *  @return The bytes, or null when a new block cannot be allocated or the
 *  size overflows. */
MULE_NODISCARD void *arena_alloc(Arena *a, size_t size, size_t align);
/** An Allocator that draws from @p a. It lives as long as @p a.
 *  @warning @p a must not move after this call.
 *  @par Example
 *  @snippet{trimleft} test_mem.c allocator */
MULE_NODISCARD const Allocator *arena_allocator(Arena *a);
/** Forget every allocation and keep the newest block for reuse. Memory
 *  handed out before is invalid afterwards. */
void arena_reset(Arena *a);
/** Release every block. Safe to call twice, and @p a stays usable. Its
 *  Allocator still works, so a Str that points at it can allocate again. */
void arena_free(Arena *a);

/** @} */

/** @} */

/* --- src/str/str.h --- */

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

/* --- src/arr/arr.h --- */

/**
 * @defgroup mod_arr Arr
 * @brief A growable array of any element type.
 *
 * Arr(T) is a struct with the same layout and rules as a Str, for any
 * element type. The operations are macros, because only a macro can work
 * for every `T`.
 *
 * @snippet{trimleft} test_arr.c arr
 *
 *   - Name each array type once with a typedef: `typedef Arr(int) IntArr;`.
 *     Every spelling of `Arr(...)` is a distinct type, so use the name.
 *   - A zero-initialised array (`IntArr xs = { 0 };`) is valid, empty, and
 *     owns no memory.
 *   - The fields are `data`, `len`, `capacity` and `alloc`, where a null
 *     `alloc` means the libc heap. Index `data` directly to read and write
 *     elements, and pass it to qsort or memcpy.
 *   - Calls that may allocate return `bool`. On false the array is
 *     unchanged.
 *   - Growing may move `data`, so pointers into it do not survive arr_push(),
 *     arr_insert() or arr_reserve().
 *
 * @warning The macros evaluate the array argument more than once. Pass a
 * variable or a plain pointer, never an expression with side effects.
 * @{
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

/** @name Growing
 *  Add elements. Each call may move `data`.
 *  @{ */

/** Make room for @p n more elements without another allocation.
 *  @return False when out of memory or the size overflows. The array is
 *  then unchanged. */
#define arr_reserve(a, n)                                                      \
    arr_reserve_(&(a)->data, &(a)->capacity, (a)->len, (n), sizeof *(a)->data, \
                 (a)->alloc)

/** Append @p v. @p v is evaluated after any growth and before `len`
 *  changes, so it may read the array itself, as in `arr_push(&xs,
 *  xs.data[0])` or `arr_push(&xs, xs.len)`.
 *  @return False when out of memory. The array is then unchanged. */
#define arr_push(a, v)                                                         \
    (arr_reserve((a), 1) ? ((a)->data[(a)->len] = (v), (a)->len++, true)       \
                         : false)

/** Insert @p v before index @p i, shifting the rest up. @p i may equal len.
 *  @return False when out of memory. The array is then unchanged.
 *  @pre `i <= len`. An assert checks it. */
#define arr_insert(a, i, v)                                                    \
    (assert(arr_index_ok_((i), (a)->len)),                                     \
     arr_reserve((a), 1) ? (memmove((a)->data + (i) + 1, (a)->data + (i),      \
                                    ((a)->len - (i)) * sizeof *(a)->data),     \
                            (a)->data[(i)] = (v), (a)->len++, true)            \
                         : false)

/** @} */

/** @name Shrinking
 *  Remove elements. None of these allocate or move `data`.
 *  @{ */

/** Remove the element at index @p i, shifting the rest down.
 *  @pre `i < len`. An assert checks it. */
#define arr_remove(a, i)                                                       \
    (assert((i) < (a)->len),                                                   \
     memmove((a)->data + (i), (a)->data + (i) + 1,                             \
             ((a)->len - (i) - 1) * sizeof *(a)->data),                        \
     (void)(a)->len--)

/** Remove and return the last element.
 *  @pre The array is not empty. An assert checks it. */
#define arr_pop(a) (assert((a)->len > 0), (a)->data[--(a)->len])

/** Set the length to zero. Keeps the allocation for reuse. */
#define arr_clear(a) ((void)((a)->len = 0))

/** Free the elements' memory and reset to empty. Keeps `alloc`, and is safe
 *  to call twice. */
#define arr_free(a)                                                            \
    (arr_free_(&(a)->data, (a)->capacity * sizeof *(a)->data, (a)->alloc),     \
     (a)->len = 0, (void)((a)->capacity = 0))

/** @} */

/** @cond */
/* A function, so a literal 0 index does not trip gcc's -Wtype-limits. */
static inline bool arr_index_ok_(size_t i, size_t len) {
    return i <= len;
}

MULE_NODISCARD bool arr_reserve_(void *data_field, size_t *capacity, size_t len,
                                 size_t add, size_t elem,
                                 const Allocator *alloc);
void arr_free_(void *data_field, size_t bytes, const Allocator *alloc);
/** @endcond */

/** @} */

/* --- src/map/map.h --- */

/**
 * @defgroup mod_map Map
 * @brief A hash map from StrView keys to entries of any struct type.
 *
 * Map(E) is an Arr(E) with a hash index beside it. The entries sit in
 * `data` in the order they were added, so a plain loop visits them, and the
 * index finds one by its key in constant time.
 *
 * @snippet{trimleft} test_map.c map_type
 * @snippet{trimleft} test_map.c map
 *
 *   - E is a struct whose first member is `StrView key`. Its other members
 *     are the value, with whatever fields the caller needs. A struct with
 *     only a key makes a set. Map(E) fails to compile when E does not start
 *     with the key.
 *   - Name each map type once with a typedef, as for Arr.
 *   - A zero-initialised map (`WordMap m = { 0 };`) is valid, empty, and
 *     owns no memory. `alloc` picks where both the entries and the index
 *     live, and null means the libc heap.
 *   - map_put() and map_find() give the index of an entry in `data`. An
 *     index stays valid as the map grows. A pointer into `data` does not.
 *   - Change any field of an entry except `key`. Changing a key or
 *     reordering `data`, as qsort does, breaks every later lookup, so sort
 *     only after the last one.
 *   - Calls that may allocate return `bool`. On false the map is unchanged.
 *
 * ## Who owns the keys
 *
 * The map stores each key as a StrView and never copies its bytes. Like any
 * view, a key is valid only while the bytes it points at stay alive and in
 * place, and the map needs every key valid for as long as it is used.
 *
 * Keys cut from one buffer that outlives the map cost nothing, as when the
 * words of a file read into a Str are counted. When the bytes do not live
 * long enough, as with a line buffer that str_read_line() reuses, copy each
 * new key into an arena before adding it, and free the arena with the map.
 *
 * @snippet{trimleft} test_map.c own_keys
 *
 * The hash is not seeded, so keys chosen by an attacker to collide can slow
 * every lookup to a linear scan.
 *
 * @warning The macros evaluate the map argument more than once. Pass a
 * variable or a plain pointer, never an expression with side effects.
 * @{
 */



/** A hash map whose entries are @p E, a struct that starts with
 *  `StrView key`. Declare it with a typedef before use. */
#define Map(E)                                                                 \
    struct {                                                                   \
        E *data;                                                               \
        size_t len;                                                            \
        size_t capacity;                                                       \
        const Allocator *alloc;                                                \
        uint64_t *slots;                                                       \
        size_t slot_count;                                                     \
        _Static_assert(offsetof(E, key) == 0 &&                                \
                           _Generic(((E *)0)->key, StrView: 1, default: 0),    \
                       "the first member of a map entry must be StrView key"); \
    }

/** The most entries a map holds: 2^31 index slots, at most 3/4 of them
 *  full. */
#define MAP_MAX ((size_t)3 << 29)

/** @name Lookup
 *  Find an entry by its key.
 *  @{ */

/** Find @p key and store its index in `*i`.
 *  @return True when @p key is in the map. False leaves `*i` as it was. */
#define map_find(m, key, i)                                                    \
    map_find_((m)->data, sizeof *(m)->data, (m)->slots, (m)->slot_count,       \
              (key), (i))

/** Find @p key, or add it at the end of `data` when it is missing, and store
 *  its index in `*i`. A new entry is all zero except for its key, so a
 *  count starts at 0. An existing entry is left as it was.
 *  @return False when out of memory, or when the map already holds
 *  #MAP_MAX entries. The map is then unchanged.
 *  @warning It may move `data`, but not when @p key is already there. */
#define map_put(m, key, i)                                                     \
    map_put_(&(m)->data, &(m)->len, &(m)->capacity, (m)->alloc, &(m)->slots,   \
             &(m)->slot_count, sizeof *(m)->data, (key), (i))

/** @} */

/** @name Capacity
 *  Make room ahead of time, empty the map, and free it.
 *  @{ */

/** Make room for @p n more entries, so the next @p n calls to map_put()
 *  do not allocate.
 *  @return False when out of memory or the size overflows. The map is then
 *  unchanged. */
#define map_reserve(m, n)                                                      \
    map_reserve_(&(m)->data, &(m)->capacity, (m)->len, (n), sizeof *(m)->data, \
                 (m)->alloc, &(m)->slots, &(m)->slot_count)

/** Remove every entry. Keeps the memory for reuse. */
#define map_clear(m)                                                           \
    (map_clear_((m)->slots, (m)->slot_count), (void)((m)->len = 0))

/** Free the entries and the index and reset to empty. Keeps `alloc`, and is
 *  safe to call twice. */
#define map_free(m)                                                            \
    (arr_free_(&(m)->data, (m)->capacity * sizeof *(m)->data, (m)->alloc),     \
     map_free_(&(m)->slots, &(m)->slot_count, (m)->alloc), (m)->len = 0,       \
     (void)((m)->capacity = 0))

/** @} */

/** @cond */
MULE_NODISCARD bool map_find_(const void *data, size_t elem,
                              const uint64_t *slots, size_t slot_count,
                              StrView key, size_t *i);
MULE_NODISCARD bool map_put_(void *data_field, size_t *len, size_t *capacity,
                             const Allocator *alloc, uint64_t **slots,
                             size_t *slot_count, size_t elem, StrView key,
                             size_t *i);
MULE_NODISCARD bool map_reserve_(void *data_field, size_t *capacity, size_t len,
                                 size_t add, size_t elem,
                                 const Allocator *alloc, uint64_t **slots,
                                 size_t *slot_count);
void map_clear_(uint64_t *slots, size_t slot_count);
void map_free_(uint64_t **slots, size_t *slot_count, const Allocator *alloc);
/** @endcond */

/** @} */

/* --- src/rand/rand.h --- */

/**
 * @defgroup mod_rand Rand
 * @brief A small, fast, seeded pseudo-random generator.
 *
 * The generator is splitmix64, so the same seed gives the same sequence on
 * every platform, unlike rand(). Use it for tests, simulations and shuffles,
 * where a failure must be reproducible from its seed.
 *
 *   - A zero-initialised Rand is valid. Set `state` to pick a seed.
 *   - Each Rand is independent and holds no global state. Copy one to fork
 *     its sequence.
 *   - rand_below() is unbiased. It rejects the few values that would make
 *     some results more likely than others, instead of a plain `% n`.
 *
 * @snippet{trimleft} test_rand.c rand
 *
 * @warning Not for secrets. The sequence is predictable from one output.
 * Use the operating system's generator for keys and tokens.
 * @{
 */


/** Generator state. Copy it to fork a sequence. */
typedef struct Rand {
    uint64_t state; /**< The seed, advanced by every call. */
} Rand;

/** The next 64 random bits. */
MULE_NODISCARD uint64_t rand_next(Rand *r);
/** A uniform value in [0, @p n).
 *  @pre @p n is not 0. An assert checks it. */
MULE_NODISCARD uint64_t rand_below(Rand *r, uint64_t n);

/** @} */

/* --- src/time/time.h --- */

/**
 * @defgroup mod_time Time
 * @brief A monotonic clock for measuring how long things take.
 *
 * time_now_ns() reads `CLOCK_MONOTONIC`. It never goes backwards, so the
 * difference between two readings is a duration even when the wall clock
 * changes. It does not tell the time of day.
 *
 * @snippet{trimleft} test_time.c time
 * @{
 */


/** Nanoseconds on a monotonic clock. It never goes backwards, but its zero is
 *  arbitrary, so only the difference between two readings means anything. */
MULE_NODISCARD uint64_t time_now_ns(void);

/** @} */

/* --- src/log/log.h --- */

/**
 * @defgroup mod_log Log
 * @brief Levelled messages for people reading a terminal or a log file.
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
 * ## Colour
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
 * @{
 */


#include <stdio.h>

/** @name Levels and output
 *  What gets written, and where.
 *  @{ */

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

/** @} */

/** @name Logging
 *  Each macro takes a printf format and its arguments, which the compiler
 *  checks.
 *  @{ */

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

/** @} */

/** @name Colour
 *  Decide whether to colour output, and the escapes to do it with.
 *  @{ */

/** True when colour escapes written to @p f will render as colour. Reads the
 *  environment on every call, so call it once and keep the answer. */
MULE_NODISCARD bool log_color_enabled(FILE *f);

/* ANSI escape sequences. Macros, so they join string literals:
 * `LOG_GREEN "ok" LOG_RESET`. Print them only when log_color_enabled(). */
#define LOG_RED "\x1b[31m"    /**< Red text. */
#define LOG_GREEN "\x1b[32m"  /**< Green text. */
#define LOG_YELLOW "\x1b[33m" /**< Yellow text. */
#define LOG_RESET "\x1b[0m"   /**< Back to the terminal's default style. */

/** @} */

/** @cond */
extern LogLevel log_level_;

#define LOG_AT_(level, ...)                                                    \
    ((level) >= LOG_LEVEL_MIN && (level) >= log_level_                         \
         ? log_write_((level), __FILE__, __LINE__, __VA_ARGS__)                \
         : (void)0)

void log_write_(LogLevel level, const char *file, int line, const char *fmt,
                ...) MULE_PRINTF(4, 5);
MULE_NORETURN void log_fatal_(const char *file, int line, const char *fmt, ...)
    MULE_PRINTF(3, 4);
/** @endcond */

/** @} */

#endif /* MULE_H */

#if defined(MULE_TEST) && !defined(MULE_TEST_DONE)
#define MULE_TEST_DONE

/* --- src/test/test.h --- */

/**
 * @defgroup mod_test Test
 * @brief A small unit-test harness that catches leaks, hangs and
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
 *
 * Every test in tests/ is written this way, so they are the best place to
 * see the harness and the library in use.
 * @{
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

/** @} */

#endif /* MULE_TEST */

#if defined(MULE_IMPLEMENTATION) && !defined(MULE_IMPLEMENTATION_DONE)
#define MULE_IMPLEMENTATION_DONE

/* --- src/mem/mem.c --- */


#include <assert.h>
#include <stdlib.h>
#include <string.h>

struct ArenaBlock {
    ArenaBlock *prev;                 /**< Older block, or null. */
    size_t used;                      /**< Bytes of `data` handed out. */
    size_t cap;                       /**< Bytes in `data`. */
    alignas(max_align_t) char data[]; /**< The memory itself. */
};

void *mem_realloc(const Allocator *a, void *old, size_t old_size,
                  size_t new_size) {
    if (a != NULL) return a->fn(a->ctx, old, old_size, new_size);
    /* realloc(p, 0) is undefined in C23, so freeing is its own branch. */
    if (new_size == 0) {
        free(old);
        return NULL;
    }
#ifdef __APPLE__
    /* macOS 26 keeps a large block grown by realloc dirty after free, so
     * growing leaked memory. A fresh block and a copy returns it. */
    if (old != NULL && new_size > old_size) {
        void *p = malloc(new_size);
        if (p == NULL) return NULL;
        memcpy(p, old, old_size);
        free(old);
        return p;
    }
#endif
    return realloc(old, new_size);
}

/* Carve `size` bytes out of `b`, or return null if they do not fit. */
static void *mem_bump_(ArenaBlock *b, size_t size, size_t align) {
    size_t pad = (size_t)-(uintptr_t)(b->data + b->used) & (align - 1);
    size_t start, end;
    if (ckd_add(&start, b->used, pad) ||
        ckd_add(&end, start, size) ||
        end > b->cap)
        return NULL;
    b->used = end;
    return b->data + start;
}

void *arena_alloc(Arena *a, size_t size, size_t align) {
    assert(align != 0 && (align & (align - 1)) == 0);
    void *p = a->head != NULL ? mem_bump_(a->head, size, align) : NULL;
    if (p == NULL) {
        /* Room for the worst-case padding too, so the bump cannot fail. */
        size_t cap, bytes;
        if (ckd_add(&cap, size, align - 1)) return NULL;
        if (cap < MEM_ARENA_BLOCK) cap = MEM_ARENA_BLOCK;
        if (ckd_add(&bytes, cap, sizeof(ArenaBlock))) return NULL;

        ArenaBlock *b = malloc(bytes);
        if (b == NULL) return NULL;
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
    if (old != NULL && old == a->last) {
        ArenaBlock *b = a->head;
        size_t start = (size_t)((char *)old - b->data);
        size_t end;
        if (new_size == 0) {
            b->used = start;
            a->last = NULL;
            return NULL;
        }
        if (!ckd_add(&end, start, new_size) && end <= b->cap) {
            b->used = end;
            return old;
        }
    }

    if (new_size == 0) return NULL; /* older allocations wait for reset */
    void *p = arena_alloc(a, new_size, alignof(max_align_t));
    if (p != NULL && old != NULL)
        memcpy(p, old, old_size < new_size ? old_size : new_size);
    return p;
}

const Allocator *arena_allocator(Arena *a) {
    a->allocator = (Allocator){ .fn = mem_arena_fn_, .ctx = a };
    return &a->allocator;
}

void arena_reset(Arena *a) {
    if (a->head == NULL) return;
    for (ArenaBlock *b = a->head->prev; b != NULL;) {
        ArenaBlock *prev = b->prev;
        free(b);
        b = prev;
    }
    a->head->prev = NULL;
    a->head->used = 0;
    a->last = NULL;
}

void arena_free(Arena *a) {
    for (ArenaBlock *b = a->head; b != NULL;) {
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
    if (needed <= s->capacity && s->data != NULL) return true;

    size_t cap = s->capacity ? s->capacity : 15;
    while (cap < needed) {
        if (ckd_mul(&cap, cap, 2)) {
            cap = needed;
            break;
        }
    }

    size_t bytes;
    if (ckd_add(&bytes, cap, 1)) return false;

    size_t old = s->data != NULL ? s->capacity + 1 : 0;
    char *p = mem_realloc(s->alloc, s->data, old, bytes);
    if (p == NULL) return false;
    if (s->data == NULL) p[0] = '\0';

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
    if (s->data != NULL)
        s->data = mem_realloc(s->alloc, s->data, s->capacity + 1, 0);
    *s = (Str){ .alloc = s->alloc };
}

bool str_reserve(Str *s, size_t additional) {
    size_t needed;
    if (ckd_add(&needed, s->len, additional)) return false;
    return str_grow_(s, needed);
}

bool str_shrink_to_fit(Str *s) {
    if (s->data == NULL || s->capacity == s->len) return true;
    char *p = mem_realloc(s->alloc, s->data, s->capacity + 1, s->len + 1);
    if (p == NULL) return false;
    s->data = p;
    s->capacity = s->len;
    return true;
}

void str_clear(Str *s) {
    s->len = 0;
    if (s->data != NULL) s->data[0] = '\0';
}

bool str_push(Str *s, char c) {
    /* Locals, because a char store may alias `*s` and force reloads. */
    char *data = s->data;
    size_t len = s->len;
    if (data != NULL && len < s->capacity) {
        data[len] = c;
        data[len + 1] = '\0';
        s->len = len + 1;
        return true;
    }
    return str_append(s, (StrView){ .data = &c, .len = 1 });
}

bool str_append(Str *s, StrView v) {
    if (v.len == 0) return true;

    /* Fast path: it fits, so the buffer cannot move under `v`. Locals, as in
     * str_push, so the stores into `data` cannot force reloads of `*s`. */
    char *data = s->data;
    size_t len = s->len;
    if (data != NULL && v.len <= s->capacity - len) {
        memmove(data + len, v.data, v.len);
        data[len + v.len] = '\0';
        s->len = len + v.len;
        return true;
    }

    /* `v` may point into our own buffer (e.g. str_append(&s, str_view(&s))).
     * Remember its offset, since growing may move the allocation. */
    const bool aliased = s->data != NULL &&
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
    size_t room = s->data != NULL ? s->capacity - s->len + 1 : 0;
    char *end = room != 0 ? s->data + s->len : NULL;
    int n = vsnprintf(end, room, fmt, ap);
    bool ok = n >= 0 && (size_t)n < room;
    if (ok) {
        s->len += (size_t)n;
    } else {
        /* The attempt overwrote the terminator; restore it before failing. */
        if (end != NULL) *end = '\0';
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

    if (ok && s->data == NULL) {
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
        if (s->data != NULL) s->data[old] = '\0';
        return false;
    }
    if (s->data == NULL) return true; /* an empty first line */
    memmove(s->data, s->data + old, s->len - old);
    s->len -= old;
    s->data[s->len] = '\0';
    return true;
}

StrView str_view(const Str *s) {
    return (StrView){ .data = str_cstr(s), .len = s->len };
}

const char *str_cstr(const Str *s) {
    return s->data != NULL ? s->data : "";
}

bool str_eq(StrView a, StrView b) {
    return a.len == b.len && (a.len == 0 || memcmp(a.data, b.data, a.len) == 0);
}

StrView sv_from_cstr(const char *cstr) {
    if (cstr == NULL) return (StrView){ .data = "", .len = 0 };
    return (StrView){ .data = cstr, .len = strlen(cstr) };
}

/* --- Views --------------------------------------------------------------- */

size_t sv_find_byte(StrView v, char c) {
    if (v.len == 0) return SV_NPOS;
    const char *p = memchr(v.data, c, v.len);
    return p != NULL ? (size_t)(p - v.data) : SV_NPOS;
}

size_t sv_find(StrView v, StrView needle) {
    if (needle.len == 0) return 0;
    if (needle.len > v.len) return SV_NPOS;

    /* memchr to each candidate first byte, then compare the rest. */
    size_t last = v.len - needle.len;
    for (size_t i = 0; i <= last; i++) {
        const char *p = memchr(v.data + i, needle.data[0], last - i + 1);
        if (p == NULL) return SV_NPOS;
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
    uint64_t set[4] = { 0 };
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
    if (p == NULL) return false;
    memcpy(data_field, &p, sizeof p);
    *capacity = cap;
    return true;
}

void arr_free_(void *data_field, size_t bytes, const Allocator *alloc) {
    void *p;
    memcpy(&p, data_field, sizeof p);
    if (p != NULL) p = mem_realloc(alloc, p, bytes, 0);
    memcpy(data_field, &p, sizeof p);
}

/* --- src/map/map.c --- */

/* The index is open addressing with linear probing over a power-of-two
 * table of uint64_t slots. A slot holds the top 32 bits of its key's hash
 * above the entry's index plus one, so 0 marks an empty slot. A probe
 * compares the stored hash before touching the entry, and growth moves
 * slots without hashing a key again. */

#include <string.h>

static uint32_t map_hash_(StrView key) {
    const char *p = key.data;
    size_t n = key.len;
    uint64_t h = 0x9e3779b97f4a7c15u ^ (uint64_t)n;
    for (; n >= 8; p += 8, n -= 8) {
        uint64_t w;
        memcpy(&w, p, sizeof w);
        h = (h ^ w) * 0xff51afd7ed558ccdu;
        h ^= h >> 32;
    }
    uint64_t w = 0;
    if (n > 0) memcpy(&w, p, n);
    h = (h ^ w) * 0xff51afd7ed558ccdu;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53u;
    h ^= h >> 33;
    return (uint32_t)(h >> 32);
}

/* Find the slot that holds `key`, or the empty slot where it would go.
 * `slot_count` must be non-zero, and the table must have an empty slot. */
static bool map_probe_(const char *data, size_t elem, const uint64_t *slots,
                       size_t slot_count, StrView key, uint32_t hash,
                       size_t *pos) {
    size_t mask = slot_count - 1;
    for (size_t p = hash & mask;; p = (p + 1) & mask) {
        uint64_t s = slots[p];
        if (s == 0) {
            *pos = p;
            return false;
        }
        if ((uint32_t)(s >> 32) != hash) continue;
        StrView k;
        memcpy(&k, data + ((uint32_t)s - 1) * elem, sizeof k);
        if (str_eq(k, key)) {
            *pos = p;
            return true;
        }
    }
}

bool map_find_(const void *data, size_t elem, const uint64_t *slots,
               size_t slot_count, StrView key, size_t *i) {
    size_t pos;
    if (slot_count == 0 ||
        !map_probe_(data, elem, slots, slot_count, key, map_hash_(key), &pos))
        return false;
    *i = (uint32_t)slots[pos] - 1;
    return true;
}

bool map_reserve_(void *data_field, size_t *capacity, size_t len, size_t add,
                  size_t elem, const Allocator *alloc, uint64_t **slots,
                  size_t *slot_count) {
    size_t needed;
    if (ckd_add(&needed, len, add)) return false;
    size_t count = *slot_count != 0 ? *slot_count : 8;
    while (needed > count / 4 * 3) {
        if (count >= (size_t)1 << 31) return false;
        count *= 2;
    }

    /* The new index comes first, so a failure leaves the entries as they
     * were. */
    uint64_t *fresh = NULL;
    size_t bytes;
    if (ckd_mul(&bytes, count, sizeof *fresh)) return false;
    if (count != *slot_count) {
        fresh = mem_realloc(alloc, NULL, 0, bytes);
        if (fresh == NULL) return false;
        memset(fresh, 0, bytes);
    }
    if (!arr_reserve_(data_field, capacity, len, add, elem, alloc)) {
        if (fresh != NULL) fresh = mem_realloc(alloc, fresh, bytes, 0);
        return false;
    }
    if (fresh == NULL) return true;

    size_t mask = count - 1;
    for (size_t j = 0; j < *slot_count; j++) {
        uint64_t s = (*slots)[j];
        if (s == 0) continue;
        size_t p = (uint32_t)(s >> 32) & mask;
        while (fresh[p] != 0) p = (p + 1) & mask;
        fresh[p] = s;
    }
    map_free_(slots, slot_count, alloc);
    *slots = fresh;
    *slot_count = count;
    return true;
}

bool map_put_(void *data_field, size_t *len, size_t *capacity,
              const Allocator *alloc, uint64_t **slots, size_t *slot_count,
              size_t elem, StrView key, size_t *i) {
    char *data;
    memcpy(&data, data_field, sizeof data);
    uint32_t hash = map_hash_(key);
    size_t pos = 0;
    if (*slot_count != 0 &&
        map_probe_(data, elem, *slots, *slot_count, key, hash, &pos)) {
        *i = (uint32_t)(*slots)[pos] - 1;
        return true;
    }

    size_t old_count = *slot_count;
    if (!map_reserve_(data_field, capacity, *len, 1, elem, alloc, slots,
                      slot_count))
        return false;
    memcpy(&data, data_field, sizeof data);
    if (*slot_count != old_count)
        (void)map_probe_(data, elem, *slots, *slot_count, key, hash, &pos);

    char *entry = data + *len * elem;
    memset(entry, 0, elem);
    memcpy(entry, &key, sizeof key);
    (*slots)[pos] = (uint64_t)hash << 32 | (uint64_t)(*len + 1);
    *i = (*len)++;
    return true;
}

void map_clear_(uint64_t *slots, size_t slot_count) {
    if (slot_count != 0) memset(slots, 0, slot_count * sizeof *slots);
}

void map_free_(uint64_t **slots, size_t *slot_count, const Allocator *alloc) {
    if (*slots != NULL)
        *slots = mem_realloc(alloc, *slots, *slot_count * sizeof **slots, 0);
    *slot_count = 0;
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
    return v != NULL && v[0] != '\0' && strcmp(v, "0") != 0;
}

bool log_color_enabled(FILE *f) {
    const char *no_color = getenv("NO_COLOR");
    if (no_color != NULL && no_color[0] != '\0') return false;
    if (log_env_on_("CLICOLOR_FORCE") || log_env_on_("FORCE_COLOR"))
        return true;
    if (!isatty(fileno(f))) return false;

    const char *term = getenv("TERM");
    return term != NULL && term[0] != '\0' && strcmp(term, "dumb") != 0;
}

MULE_PRINTF(4, 0)
static FILE *log_vwrite_(LogLevel level, const char *file, int line,
                         const char *fmt, va_list ap) {
    FILE *out = log_out_ != NULL ? log_out_ : stderr;
    if (log_color_ < 0) log_color_ = log_color_enabled(out);
    const char *on = log_color_ ? log_colors_[level] : "";
    const char *off = on[0] != '\0' ? LOG_RESET : "";

    /* shortcut: one allocation per line. Reuse a thread_local buffer if
     * logging ever shows up in a profile. */
    Str s = { 0 };
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
