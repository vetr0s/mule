/* MULE: Multi-Use Library Essentials, as one header.
 *
 * Generated from src/ by `./build dist`. Edit src/ and regenerate.
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
 * Define MULE_TEST as well to get the test harness. test_init installs its
 * counting, failing Allocator as the heap, so its leak counting and allocation
 * failure cover the whole library.
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
#define MULE_VERSION_MINOR 2
/** Bumped for fixes that change no behaviour a program relies on. */
#define MULE_VERSION_PATCH 0
/** All three numbers as a string, such as "0.0.1". */
#define MULE_VERSION "0.2.0"
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
 * All allocation in the library goes through mem_realloc(). Outside the test
 * harness, only this module calls malloc, realloc and free. mem_set_heap()
 * sends what would reach them to an Allocator instead, which is how the test
 * harness counts allocations and fails them on demand.
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
 *   - arena_mark() saves a position and arena_restore() rewinds to it, so a
 *     function can free just what it allocated. Restore frees the blocks
 *     added since the mark and leaves older allocations intact. Marks nest,
 *     and restore goes last in, first out. A stale or out-of-order mark is
 *     undefined, as a freed pointer is.
 *   - Taking a mark stops the previous allocation from growing in place, so
 *     a buffer never straddles the mark.
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
 *  @param a  The allocator, or null for the heap: libc, unless mem_set_heap()
 *  installed another.
 *  @param old  The block to resize, or null to allocate a new one.
 *  @param old_size  The size @p old was allocated with. Ignored when @p old
 *  is null.
 *  @param new_size  The size wanted. Zero frees @p old.
 *  @return The new block, or null on failure with @p old untouched. A free
 *  returns null too, so `p = mem_realloc(a, p, n, 0);` frees and clears. */
MULE_NODISCARD void *mem_realloc(const Allocator *a, void *old, size_t old_size,
                                 size_t new_size);

/** Make @p heap serve every null allocator, including arena blocks. The
 *  setting is process-wide and not thread safe, and is meant for a test
 *  harness. Memory must be freed through the heap that allocated it.
 *  @param heap  The Allocator to install, or null for libc. It must outlive
 *  the setting, and its blocks must be aligned for `max_align_t`.
 *  @return The previous setting, so the caller can put it back. */
const Allocator *mem_set_heap(const Allocator *heap);

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

/** A position in an arena, from arena_mark(). The zero value is the mark of
 *  an empty arena. */
typedef struct ArenaMark {
    ArenaBlock *head; /**< Newest block at the mark. */
    size_t used;      /**< Bytes handed out of `head` at the mark. */
} ArenaMark;

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
/** Save the current position of @p a for arena_restore(). The newest
 *  allocation stops growing in place, so growing it later moves it past the
 *  mark.
 *  @param a  The arena.
 *  @return The mark.
 *  @warning arena_reset() and arena_free() kill every mark taken before them.
 *  Passing a dead mark to arena_restore() is undefined. */
MULE_NODISCARD ArenaMark arena_mark(Arena *a);
/** Free everything allocated since @p mark and rewind to it. Allocations made
 *  before the mark keep their contents. The next arena_alloc() of the same
 *  size and alignment returns the address the first one after the mark got.
 *  @param a  The arena @p mark came from.
 *  @param mark  A live mark. Restoring it kills the marks taken after it.
 *  @warning Memory handed out since the mark is invalid afterwards. That
 *  includes the buffer of a Str that grew in the arena since the mark, even
 *  if the Str itself is older. */
void arena_restore(Arena *a, ArenaMark mark);
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
 *     not the bytes, and freeing both is a double free. Move the bytes with
 *     `Str b = str_take(&a);`, which leaves `a` empty.
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
/** Move the buffer out of @p s. The result owns the bytes, and @p s is left
 *  empty with no memory. It keeps `alloc` in both, and never allocates.
 *  @par Example
 *  @snippet{trimleft} test_str.c take */
MULE_NODISCARD Str str_take(Str *s);

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
/** Index of the last @p needle in @p v. An empty needle is at `v.len`.
 *  @return The index, or #SV_NPOS when @p needle does not occur. */
MULE_NODISCARD size_t sv_find_last(StrView v, StrView needle);
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
/** @p v without leading ASCII whitespace, as sv_trim() counts it. */
MULE_NODISCARD StrView sv_trim_left(StrView v);
/** @p v without trailing ASCII whitespace, as sv_trim() counts it. */
MULE_NODISCARD StrView sv_trim_right(StrView v);

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
/** Parse all of @p v as an unsigned integer in @p base. Digits past 9 are
 *  letters in either case.
 *  @param v  The whole text to parse.
 *  @param base  The base, from 2 to 36.
 *  @param[out] out  The value. Unchanged when the call returns false.
 *  @pre @p base is in 2..36. An assert checks it.
 *  @return False on an empty view, a sign, whitespace, a prefix such as `0x`,
 *  a byte that is not a digit of @p base, or a value above `UINT64_MAX`.
 *  @par Example
 *  @snippet{trimleft} test_str.c parse_u64 */
MULE_NODISCARD bool sv_parse_u64(StrView v, int base, uint64_t *out);
/** Parse all of @p v as a double, in any form strtod accepts in the C
 *  locale.
 *  @param v  The whole text to parse.
 *  @param[out] out  The value. Unchanged when the call returns false.
 *  @return False on whitespace, trailing bytes, overflow to infinity, or a
 *  view over 255 bytes. */
MULE_NODISCARD bool sv_parse_f64(StrView v, double *out);

/** @} */

/** @} */

/* --- src/err/err.h --- */

/**
 * @defgroup mod_err Err
 * @brief Why a call failed, for functions that can fail in more than one way.
 *
 * Most functions in MULE fail only when memory runs out, so their `bool`
 * says everything. A function that opens a file, runs a program or parses
 * input can fail for several reasons, and the caller needs to know which.
 * Such a function takes an `Err *` as its last argument.
 *
 *   - It still returns `bool`, so calls chain with `&&` as they do
 *     everywhere else.
 *   - On false it fills the Err. The Err is the one argument a failed call
 *     changes.
 *   - On true it leaves the Err alone, so a chain keeps its first failure.
 *   - A null `Err *` means the caller wants only the `bool`.
 *   - A zero-initialised Err (`Err err = { 0 };`) holds #ERR_NONE and an
 *     empty message.
 *   - The message lives inside the Err. Nothing is allocated, so there is
 *     nothing to free, and running out of memory can still be reported.
 *
 * An Err has three parts. `kind` is portable and is what code branches on.
 * `code` is the raw number behind it, and `msg` is a sentence for a person.
 *
 * A caller's own function can report in the same style:
 *
 * @snippet{trimleft} test_err.c load_config
 *
 * The caller branches on the kind and prints the message:
 *
 * @snippet{trimleft} test_err.c report
 *
 * Read `errno` straight after the call that set it, and pass it in. A later
 * call such as `fclose` may overwrite it.
 * @{
 */


/** @name Errors
 *  The error value and the functions that fill it.
 *  @{ */

/** Bytes in Err::msg, including the terminator. A longer message is cut. */
#define ERR_MSG_MAX 256

/** What went wrong, in terms that are the same on every platform. */
typedef enum ErrKind {
    ERR_NONE = 0,   /**< No error. A zeroed Err holds this. */
    ERR_NO_MEMORY,  /**< An allocation failed or a size overflowed. */
    ERR_NOT_FOUND,  /**< A file, directory or program does not exist. */
    ERR_EXISTS,     /**< Something is already where a new one should go. */
    ERR_PERMISSION, /**< The process may not do this. */
    ERR_NOT_DIR,    /**< A directory was needed and something else is there. */
    ERR_IS_DIR,     /**< A directory is where something else was needed. */
    ERR_INVALID,    /**< The input was malformed, such as an unknown flag. */
    ERR_EXIT,   /**< A child process exited nonzero. `code` is its status. */
    ERR_SIGNAL, /**< A signal killed a child. `code` is the signal. */
    ERR_OS,     /**< Any other system failure. `code` is the raw value. */
} ErrKind;

/** Why a call failed. Zero-initialise it, pass its address, and read it
 *  after a call returns false. */
typedef struct Err {
    ErrKind kind;          /**< What went wrong. Branch on this. */
    int code;              /**< `errno`, an exit status or a signal number. */
    char msg[ERR_MSG_MAX]; /**< A sentence for a person. Always terminated. */
} Err;

/** Fill @p e with @p kind, @p code and a printf-formatted message. Does
 *  nothing when @p e is null.
 *  @param e  The error to fill, or null.
 *  @param kind  What went wrong.
 *  @param code  The raw number behind it, or 0.
 *  @param fmt  A printf format for the message, and its arguments after it. */
void err_set(Err *e, ErrKind kind, int code, const char *fmt, ...)
    MULE_PRINTF(4, 5);

/** Fill @p e from an OS error code. It maps @p code to an ErrKind and writes
 *  `op subject: reason`, where the reason is the system's text for @p code.
 *  Does nothing when @p e is null.
 *  @param e  The error to fill, or null.
 *  @param code  `errno` on Unix, read straight after the failing call.
 *  @param op  What was attempted, such as "open" or "mkdir".
 *  @param subject  What it was attempted on, usually a path. */
void err_os(Err *e, int code, const char *op, StrView subject);

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
 * index finds one by its key in constant time. map_remove() breaks that
 * order, because it moves the last entry into the hole.
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
 *     map_remove() gives the last entry the removed one's index.
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

/** @name Remove
 *  Delete an entry by its key.
 *  @{ */

/** Remove @p key. The last entry moves into its place in `data`, so
 *  `data` stays dense. It never allocates.
 *  @return False when @p key is not in the map. The map is then unchanged.
 *  @warning The last entry changes index. `data` does not move, but a
 *  pointer at the removed index now points at the old last entry, and a
 *  pointer at the old last entry points past `len`. */
#define map_remove(m, key)                                                     \
    map_remove_((m)->data, &(m)->len, (m)->slots, (m)->slot_count,             \
                sizeof *(m)->data, (key))

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
MULE_NODISCARD bool map_remove_(void *data, size_t *len, uint64_t *slots,
                                size_t slot_count, size_t elem, StrView key);
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

/* --- src/fs/fs.h --- */

/**
 * @defgroup mod_fs Fs
 * @brief Files, directories and paths.
 *
 * Every function that reaches the operating system takes its paths as
 * `const char *`, because the system calls need NUL-terminated strings.
 * The path functions only cut a path into pieces, so they take and return
 * StrViews and never allocate. See [Views or C strings](@ref cstrings).
 *
 *   - A function that can fail takes an `Err *` last. Its message names
 *     the path where the failure happened.
 *   - A failed call may already have changed the disk. Only
 *     fs_write_atomic() is all or nothing. Each other function says under
 *     *Returns* what a failure can leave behind.
 *   - A Str argument, as in fs_read(), is unchanged when the call fails.
 *   - fs_walk() visits each directory's entries sorted by name, so its order
 *     is the same on every system.
 *
 * @snippet{trimleft} test_fs.c fs
 * @{
 */


/** @name Information
 *  What is at a path.
 *  @{ */

/** What kind of thing a path names. */
typedef enum FsType {
    FS_MISSING = 0, /**< Nothing is there. A zeroed FsStat holds this. */
    FS_FILE,        /**< A regular file. */
    FS_DIR,         /**< A directory. */
    FS_LINK,        /**< A symbolic link, from fs_lstat() or fs_walk(). */
    FS_OTHER,       /**< A device, pipe or socket. */
} FsType;

/** What fs_stat() and fs_lstat() report. */
typedef struct FsStat {
    FsType type;      /**< What the path names. */
    uint64_t size;    /**< Size in bytes. */
    int64_t mtime_ns; /**< Last modification, in nanoseconds since 1970. */
    uint32_t mode;    /**< Permission bits, such as 0644. */
} FsStat;

/** Describe what @p path names, following symbolic links.
 *  @return False when @p path does not exist (#ERR_NOT_FOUND) or cannot be
 *  read. @p st is then unchanged. */
MULE_NODISCARD bool fs_stat(const char *path, FsStat *st, Err *err);
/** Like fs_stat(), but a symbolic link is described as #FS_LINK rather
 *  than followed.
 *  @return False as for fs_stat(). */
MULE_NODISCARD bool fs_lstat(const char *path, FsStat *st, Err *err);
/** True when @p path names something, following symbolic links. A link to
 *  nothing does not exist. */
MULE_NODISCARD bool fs_exists(const char *path);

/** @} */

/** @name Reading and writing
 *  Whole files at once.
 *  @{ */

/** Append the whole file at @p path to @p out.
 *  @return False when the file cannot be opened or read, or memory runs
 *  out. @p out is then unchanged. */
MULE_NODISCARD bool fs_read(Str *out, const char *path, Err *err);
/** Write @p data to @p path, creating the file or replacing its contents.
 *  @return False when the file cannot be opened or written. A file that
 *  existed may then hold part of @p data. */
MULE_NODISCARD bool fs_write(const char *path, StrView data, Err *err);
/** Write @p data to @p path so that a reader sees the old file or the new
 *  one, never a mixture. It writes a temporary file beside @p path, flushes
 *  it to disk and renames it into place. The file keeps its permissions,
 *  and a new file gets 0644.
 *  @return False on any failure. @p path is then as it was, and the
 *  temporary file is gone. */
MULE_NODISCARD bool fs_write_atomic(const char *path, StrView data, Err *err);

/** @} */

/** @name Changing the tree
 *  Make, remove, move and copy.
 *  @{ */

/** Make the directory @p path and any missing parents. A directory that
 *  already exists is success.
 *  @return False when a file is in the way (#ERR_EXISTS for @p path itself,
 *  #ERR_NOT_DIR for a parent), or a directory cannot be made. The parents
 *  made before the failure stay. */
MULE_NODISCARD bool fs_mkdir_p(const char *path, Err *err);
/** Remove a file, a symbolic link or an empty directory.
 *  @return False when @p path does not exist (#ERR_NOT_FOUND), is a
 *  directory with entries, or cannot be removed. */
MULE_NODISCARD bool fs_remove(const char *path, Err *err);
/** Remove @p path and everything under it. Symbolic links are removed, not
 *  followed. A missing @p path is success.
 *  @return False when something cannot be removed. Whatever was removed
 *  before the failure stays removed. */
MULE_NODISCARD bool fs_remove_all(const char *path, Err *err);
/** Move @p from to @p to, replacing a file already at @p to. Both must be
 *  on the same file system.
 *  @return False when the move fails. Nothing has changed then. */
MULE_NODISCARD bool fs_rename(const char *from, const char *to, Err *err);
/** Copy the file @p from to @p to with its permissions, replacing a file
 *  already at @p to.
 *  @return False when either file cannot be opened, read or written. @p to
 *  may then hold part of the copy. */
MULE_NODISCARD bool fs_copy(const char *from, const char *to, Err *err);
/** Copy the tree at @p from to @p to. Directories are made, files are
 *  copied with fs_copy(), and symbolic links are recreated as links.
 *  @return False on the first failure. What was copied before it stays. */
MULE_NODISCARD bool fs_copy_tree(const char *from, const char *to, Err *err);

/** @} */

/** @name Walking
 *  Visit a tree.
 *  @{ */

/** What fs_walk() does after a visit. */
typedef enum FsWalkAct {
    FS_WALK_NEXT, /**< Go on, into this directory if it is one. */
    FS_WALK_SKIP, /**< Go on, but not into this directory. */
    FS_WALK_STOP, /**< End the walk. fs_walk() returns true. */
} FsWalkAct;

/** Called once for each path fs_walk() visits. @p path is valid only
 *  during the call. */
typedef FsWalkAct FsWalkFn(void *ctx, const char *path, FsType type);

/** Visit @p root and everything under it, depth first, with each
 *  directory's entries in byte order. Paths start with @p root. Symbolic
 *  links are visited as #FS_LINK and not followed.
 *  @param root  Where to start.
 *  @param fn  Called for each path.
 *  @param ctx  Passed to @p fn unchanged.
 *  @param err  The error to fill, or null.
 *  @return False when @p root is missing or a directory cannot be read. */
MULE_NODISCARD bool fs_walk(const char *root, FsWalkFn *fn, void *ctx,
                            Err *err);

/** @} */

/** @name Paths
 *  Cut a path into pieces, and build one. Only fs_join() allocates.
 *  @{ */

/** Append the absolute path of the running program to @p out. Unlike
 *  `argv[0]`, it is right when the program was found on `PATH`.
 *  @return False when the system cannot say, or memory runs out. @p out is
 *  then unchanged. */
MULE_NODISCARD bool fs_exe_path(Str *out, Err *err);
/** The last component of @p path, ignoring trailing slashes: `b.c` for
 *  `a/b.c`. A path of only slashes gives `/`. */
MULE_NODISCARD StrView fs_basename(StrView path);
/** Everything before the last component, ignoring trailing slashes: `a` for
 *  `a/b.c`. A path with no slash gives `.`, and one directly under the root
 *  gives `/`. */
MULE_NODISCARD StrView fs_dirname(StrView path);
/** The extension of the last component, from its last dot: `.c` for
 *  `a/b.c`. Empty when there is no dot, or the only dot starts the name, as
 *  in `.build`. */
MULE_NODISCARD StrView fs_extension(StrView path);
/** Append @p part to @p path, with one `/` between them. An empty @p part
 *  adds nothing.
 *  @return False when memory runs out. @p path is then unchanged. */
MULE_NODISCARD bool fs_join(Str *path, StrView part);

/** @} */

/** @} */

/* --- src/args/args.h --- */

/**
 * @defgroup mod_args Args
 * @brief Command-line options, positional arguments and subcommands.
 *
 * Describe the options in a table of ArgOpt, each pointing at the variable
 * it sets, and pass the table to args_parse() in an Args. The same table
 * writes the `--help` text with args_help().
 *
 * @snippet{trimleft} test_args.c args
 *
 * args_parse() understands:
 *   - `--name=value` and `--name value`, `-n value` and `-nvalue`.
 *   - `--flag` and `-f`, and flags grouped as `-vq`.
 *   - `--`, after which every argument is positional.
 *   - `-h` and `--help`, which set `help` unless an option claims them.
 *   - With subcommands in `cmds`, the first positional must name one, and
 *     lands in `cmd`. The other positionals land in `rest`.
 *
 * It checks every argument before setting anything, so a failed parse
 * leaves every variable as it was. String values point into `argv`, so
 * they live as long as `argv` does.
 * @{
 */


/** @name Describing a command line
 *  The tables args_parse() reads.
 *  @{ */

/** What an option's `out` points at. */
typedef enum ArgKind {
    ARG_FLAG, /**< A `bool`, set to true when the option appears. */
    ARG_STR,  /**< A `const char *`, set to the value. */
    ARG_U64,  /**< A `uint64_t`, set to the value read in base 10. */
} ArgKind;

/** One option. */
typedef struct ArgOpt {
    const char *name;       /**< The long name without `--`, or null. */
    char short_name;        /**< The letter after `-`, or 0 for none. */
    ArgKind kind;           /**< What `out` points at. */
    void *out;              /**< The variable the option sets. */
    const char *value_name; /**< Shown in the help, such as "N". */
    const char *help;       /**< One line for the help. */
} ArgOpt;

/** One subcommand. */
typedef struct ArgCmd {
    const char *name; /**< What the user types. */
    const char *help; /**< One line for the help. */
} ArgCmd;

/** Positional arguments, borrowed from `argv`. */
typedef Arr(const char *) ArgList;

/** A command line's description, and what parsing it found. A zeroed Args
 *  takes no options, and every argument is positional. */
typedef struct Args {
    const char *usage;  /**< The usage line for the help, after "usage: ". */
    const ArgOpt *opts; /**< The options. */
    size_t opt_count;   /**< Entries in `opts`. */
    const ArgCmd *cmds; /**< The subcommands, or null for none. */
    size_t cmd_count;   /**< Entries in `cmds`. */
    const char *cmd;    /**< Set to the subcommand given, or left null. */
    bool help;          /**< Set when `-h` or `--help` appears. */
    ArgList rest;       /**< Positional arguments are appended here. */
} Args;

/** @} */

/** @name Parsing
 *  Read a command line, and describe it.
 *  @{ */

/** Parse `argv[1]` to `argv[argc - 1]` against @p a.
 *  @return False with #ERR_INVALID for an unknown option or subcommand, a
 *  missing value, a value given to a flag, or a number that does not
 *  parse, and with #ERR_NO_MEMORY when `rest` cannot grow. The message
 *  names the argument. Every variable and @p a are then unchanged. */
MULE_NODISCARD bool args_parse(Args *a, int argc, char **argv, Err *err);
/** Append the help text for @p a to @p out: the usage line, the
 *  subcommands and the options, each with its help line.
 *  @return False when memory runs out. @p out is then unchanged. */
MULE_NODISCARD bool args_help(Str *out, const Args *a);
/** Free `rest`. Safe to call twice. */
void args_free(Args *a);

/** @} */

/** @} */

/* --- src/proc/proc.h --- */

/**
 * @defgroup mod_proc Proc
 * @brief Run other programs, wait for them, and run several at once.
 *
 * A Cmd is an Arr of argument strings, the program first. It borrows them,
 * so each string must outlive every run of the command. cmd_run() starts
 * the program and waits for it. proc_start() starts it and returns a Proc
 * to wait on later, and a ProcPool caps how many run at once.
 *
 * @snippet{trimleft} test_proc.c proc
 *
 *   - The program is found on `PATH` when its name has no slash.
 *   - A run succeeds when the program exits 0. Otherwise the Err says why:
 *     #ERR_NOT_FOUND when the program does not exist, #ERR_EXIT with the
 *     exit status in `code`, or #ERR_SIGNAL with the signal number.
 *   - The child inherits stdin, stdout and stderr unless a CmdIo redirects
 *     them to files.
 *
 * A Proc is a handle to look at through these functions only. Its field
 * holds a process id today and will hold a Windows handle in 0.6.0.
 * @{
 */


/** @name Commands
 *  Build a command and print it.
 *  @{ */

/** A program and its arguments, borrowed. Zero-initialise it and free it
 *  with arr_free(). */
typedef Arr(const char *) Cmd;

/** Append any number of `const char *` arguments to the Cmd at @p c.
 *  @return False when out of memory. The Cmd is then unchanged. */
#define cmd_append(c, ...)                                                     \
    cmd_append_((c), (const char *const[]){ __VA_ARGS__ },                     \
                sizeof((const char *const[]){ __VA_ARGS__ }) /                 \
                    sizeof(const char *))

/** Append @p c to @p out as a shell command line, quoting each argument
 *  that needs it, so it can be pasted into a shell.
 *  @return False when out of memory. @p out is then unchanged. */
MULE_NODISCARD bool cmd_render(Str *out, const Cmd *c);

/** @} */

/** @name Running
 *  Start a program, and wait for it.
 *  @{ */

/** Files to connect to a child's standard streams. A null path leaves the
 *  stream as the parent's. stdout and stderr may name the same file. */
typedef struct CmdIo {
    const char *in;  /**< Read as stdin. */
    const char *out; /**< Created or truncated, and written as stdout. */
    const char *err; /**< Created or truncated, and written as stderr. */
} CmdIo;

/** A running program. Zero means none. */
typedef struct Proc {
    int64_t id_; /**< Internal. */
} Proc;

/** Start @p c without waiting for it.
 *  @param p  Set to the new process.
 *  @param c  The command. It must not be empty.
 *  @param io  Redirections, or null for none.
 *  @param err  The error to fill, or null.
 *  @return False when a redirected file cannot be opened, the program
 *  cannot be started, or @p c is empty (#ERR_INVALID). @p p is then
 *  unchanged. */
MULE_NODISCARD bool proc_start(Proc *p, const Cmd *c, const CmdIo *io,
                               Err *err);
/** Wait for @p p to end, and set it to zero.
 *  @return False when it exited nonzero or was killed by a signal, or @p p
 *  is zero (#ERR_INVALID). */
MULE_NODISCARD bool proc_wait(Proc *p, Err *err);
/** Start @p c and wait for it.
 *  @return False as for proc_start() and proc_wait(). */
MULE_NODISCARD bool cmd_run(const Cmd *c, Err *err);
/** Run @p c and append what it writes to stdout to @p out.
 *  @return False as for cmd_run(), or when the output cannot be read or
 *  stored. @p out is then unchanged. */
MULE_NODISCARD bool cmd_capture(const Cmd *c, Str *out, Err *err);

/** @} */

/** @name Pools
 *  Run several commands at once.
 *  @{ */

/** A job in a pool. */
typedef struct ProcJob {
    Proc proc;        /**< The running process. */
    const char *name; /**< The program, borrowed from the Cmd, for messages. */
} ProcJob;

/** The jobs a pool is running. */
typedef Arr(ProcJob) ProcJobs;

/** Runs commands in parallel, at most `max` at a time. Zero-initialise it,
 *  and set `max` or leave it 0 for the number of CPUs. */
typedef struct ProcPool {
    ProcJobs running; /**< Started and not yet waited for, oldest first. */
    size_t max;       /**< How many may run at once. 0 means the CPU count. */
} ProcPool;

/** Start @p c in @p pool. When the pool is full it first waits for the
 *  oldest job.
 *  @return False when that job failed, which stops the build early, or
 *  when @p c cannot be started. @p c has not started then. Call
 *  proc_pool_wait() to wait for the rest. */
MULE_NODISCARD bool proc_pool_start(ProcPool *pool, const Cmd *c, Err *err);
/** Wait for every job in @p pool.
 *  @return False when any of them failed. The Err describes the first. */
MULE_NODISCARD bool proc_pool_wait(ProcPool *pool, Err *err);
/** Wait for any jobs still running, ignoring how they end, and free the
 *  pool's memory. */
void proc_pool_free(ProcPool *pool);

/** @} */

/** @cond */
MULE_NODISCARD bool cmd_append_(Cmd *c, const char *const *args, size_t n);
/** @endcond */

/** @} */

/* --- src/build/build.h --- */

/**
 * @defgroup mod_build Build
 * @brief Write a project's build as a C program that rebuilds itself.
 *
 * A project keeps a `build.c` that includes `mule.h`, and bootstraps it
 * once with nothing but a C compiler:
 *
 *     cc build.c && ./a.out          # or: cc -o build build.c && ./build
 *
 * From then on MULE_REBUILD_SELF() recompiles the build program whenever
 * `build.c` or anything it includes has changed, and runs the new one in
 * its place. Output goes in `.build/` beside the program.
 *
 * @snippet{trimleft} project/build.c build
 *
 *   - Staleness compares modification times to the nanosecond, so an edit
 *     made in the same second as the last build still counts.
 *   - build_needs_rebuild_deps() reads the depfile a compiler writes with
 *     `-MMD -MF`, so a header edit rebuilds everything that includes it.
 *   - build_run() and build_start() log each command, quoted so it can be
 *     pasted into a shell, before running it.
 *   - A CompDb collects compile commands into `compile_commands.json`, for
 *     clangd and other tools.
 * @{
 */


/** @name Staleness
 *  Decide what needs building.
 *  @{ */

/** Set `*needed` when @p out is missing or older than any of @p inputs.
 *  @param out  The file that gets built.
 *  @param inputs  The files it is built from.
 *  @param n  Entries in @p inputs.
 *  @param needed  Set to whether @p out must be rebuilt.
 *  @param err  The error to fill, or null.
 *  @return False when an input is missing (#ERR_NOT_FOUND) or cannot be
 *  read. `*needed` is then unchanged. */
MULE_NODISCARD bool build_needs_rebuild(const char *out,
                                        const char *const *inputs, size_t n,
                                        bool *needed, Err *err);

/** Prerequisites listed in a depfile, borrowed from `text`. */
typedef Arr(const char *) DepList;

/** A depfile's prerequisites. Zero-initialise it and free it with
 *  build_deps_free(). */
typedef struct Deps {
    Str text;      /**< The prerequisites, each NUL-terminated. */
    DepList paths; /**< One pointer into `text` per prerequisite. */
} Deps;

/** Read the first rule of a depfile, as `cc -MMD -MF` writes one, into
 *  @p d. It handles continued lines, `\ ` in names and `$$`.
 *  @return False when the file cannot be read or memory runs out. @p d is
 *  then unchanged. */
MULE_NODISCARD bool build_read_deps(Deps *d, const char *depfile, Err *err);
/** Free @p d and reset it to empty. Safe to call twice. */
void build_deps_free(Deps *d);
/** Set `*needed` when @p out is missing, @p depfile is missing, or any
 *  prerequisite in @p depfile is newer than @p out or gone. A missing
 *  prerequisite means rebuild rather than fail, because a header that was
 *  deleted is no longer included.
 *  @return False when a file cannot be read. `*needed` is then unchanged. */
MULE_NODISCARD bool build_needs_rebuild_deps(const char *out,
                                             const char *depfile, bool *needed,
                                             Err *err);

/** @} */

/** @name Rebuilding itself
 *  Keep the build program up to date.
 *  @{ */

/** Rebuild the running program from the file this macro is written in,
 *  when that file or a header it includes has changed. Call it first in
 *  main. See build_rebuild_self(). */
#define MULE_REBUILD_SELF(argc, argv)                                          \
    build_rebuild_self((argc), (argv), __FILE__, NULL)

/** Rebuild the running program from @p source when it is stale, then run
 *  the new program with the same arguments in place of this one, so its
 *  exit code is the caller's. Returns only when nothing needed rebuilding,
 *  or @p source cannot be found from the current directory.
 *
 *  The program is stale when its depfile, `.build/<name>.d` beside it, is
 *  missing or lists a newer file. It compiles with `$CC`, or `cc`, and
 *  `-Wall -Wextra -MMD`. The old program waits in `.build/<name>.old`, and
 *  goes back in place when the compile fails, which exits with status 1.
 *  @param argc  main's argc. Unused, and kept so the call reads like main.
 *  @param argv  main's argv, passed to the new program.
 *  @param source  The program's source, as a path from the current
 *  directory.
 *  @param flags  More compiler arguments, such as `-Isrc`, or null. */
void build_rebuild_self(int argc, char **argv, const char *source,
                        const Cmd *flags);

/** @} */

/** @name Running
 *  Run commands and record them.
 *  @{ */

/** Log @p c with log_info(), quoted, then run it with cmd_run().
 *  @return False as for cmd_run(). */
MULE_NODISCARD bool build_run(const Cmd *c, Err *err);
/** Log @p c with log_info(), quoted, then start it with proc_pool_start().
 *  @return False as for proc_pool_start(). */
MULE_NODISCARD bool build_start(ProcPool *pool, const Cmd *c, Err *err);

/** Compile commands, for `compile_commands.json`. Zero-initialise it and
 *  free it with build_compdb_free(). */
typedef struct CompDb {
    Str json; /**< One JSON fragment per command, each ending in a newline. */
} CompDb;

/** Record that @p c compiles @p file.
 *  @return False when out of memory. @p db is then unchanged. */
MULE_NODISCARD bool build_compdb_add(CompDb *db, const char *file,
                                     const Cmd *c);
/** Write @p db to @p path as a JSON compilation database, with the current
 *  directory as each entry's `directory`. The file is left alone when it
 *  already holds the same text, so tools that watch it do not reload.
 *  @return False when the directory or the file cannot be read or written,
 *  or memory runs out. */
MULE_NODISCARD bool build_compdb_write(const CompDb *db, const char *path,
                                       Err *err);
/** Free @p db. Safe to call twice. */
void build_compdb_free(CompDb *db);

/** @} */

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
 *   - test_init() installs a counting Allocator with mem_set_heap(), so every
 *     null allocator and every arena block goes through it. RUN fails a test
 *     that ends with more live allocations than it started with, so leaks fail
 *     on every platform. A direct call to malloc or free is not counted.
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

/* --- Allocator ----------------------------------------------------------- */

static long test_live_allocs_ = 0;
static long test_alloc_budget_ = -1; /* allocations left; -1 means no limit */

/** @endcond */

/** Let the next @p n allocations succeed and fail every one after. Pass -1
 *  to stop failing. RUN resets it after each test. */
MULE_UNUSED static void test_fail_alloc_after(long n) {
    test_alloc_budget_ = n;
}

/** @cond */

static void *test_heap_fn_(void *ctx, void *old, size_t old_size,
                           size_t new_size) {
    (void)ctx;
    (void)old_size;
    if (new_size == 0) {
        if (old != NULL) test_live_allocs_--;
        free(old);
        return NULL;
    }
    if (test_alloc_budget_ == 0) return NULL;
    if (test_alloc_budget_ > 0) test_alloc_budget_--;
    void *p = realloc(old, new_size);
    if (p != NULL && old == NULL) test_live_allocs_++;
    return p;
}

static const Allocator test_heap_ = { .fn = test_heap_fn_ };

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
    mem_set_heap(&test_heap_);
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

static const Allocator *mem_heap_;

const Allocator *mem_set_heap(const Allocator *heap) {
    const Allocator *prev = mem_heap_;
    mem_heap_ = heap;
    return prev;
}

void *mem_realloc(const Allocator *a, void *old, size_t old_size,
                  size_t new_size) {
    if (a == NULL) a = mem_heap_;
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

        ArenaBlock *b = mem_realloc(NULL, NULL, 0, bytes);
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

/* Free the blocks from `b` back to `stop`, which stays. */
static void mem_free_blocks_(ArenaBlock *b, const ArenaBlock *stop) {
    while (b != stop) {
        ArenaBlock *prev = b->prev;
        b = mem_realloc(NULL, b, sizeof(ArenaBlock) + b->cap, 0);
        b = prev;
    }
}

void arena_reset(Arena *a) {
    if (a->head == NULL) return;
    mem_free_blocks_(a->head->prev, NULL);
    a->head->prev = NULL;
    a->head->used = 0;
    a->last = NULL;
}

ArenaMark arena_mark(Arena *a) {
    /* A buffer that grew in place past the mark would be cut off by restore. */
    a->last = NULL;
    return (ArenaMark){ .head = a->head,
                        .used = a->head != NULL ? a->head->used : 0 };
}

void arena_restore(Arena *a, ArenaMark mark) {
    mem_free_blocks_(a->head, mark.head);
    a->head = mark.head;
    if (a->head != NULL) a->head->used = mark.used;
    a->last = NULL;
}

void arena_free(Arena *a) {
    mem_free_blocks_(a->head, NULL);
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

Str str_take(Str *s) {
    Str taken = *s;
    *s = (Str){ .alloc = s->alloc };
    return taken;
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

size_t sv_find_last(StrView v, StrView needle) {
    if (needle.len > v.len) return SV_NPOS;
    if (needle.len == 0) return v.len;
    for (size_t i = v.len - needle.len + 1; i-- > 0;) {
        if (memcmp(v.data + i, needle.data, needle.len) == 0) return i;
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

StrView sv_trim_left(StrView v) {
    size_t from = 0;
    while (from < v.len && sv_space_(v.data[from])) from++;
    return sv_slice(v, from, v.len);
}

StrView sv_trim_right(StrView v) {
    size_t to = v.len;
    while (to > 0 && sv_space_(v.data[to - 1])) to--;
    return sv_slice(v, 0, to);
}

StrView sv_trim(StrView v) {
    return sv_trim_right(sv_trim_left(v));
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

bool sv_parse_u64(StrView v, int base, uint64_t *out) {
    assert(base >= 2 && base <= 36);
    if (v.len == 0) return false;

    uint64_t val = 0;
    for (size_t i = 0; i < v.len; i++) {
        unsigned c = (unsigned char)v.data[i], d;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'z')
            d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z')
            d = c - 'A' + 10;
        else
            return false;
        if (d >= (unsigned)base ||
            ckd_mul(&val, val, (uint64_t)base) ||
            ckd_add(&val, val, d))
            return false;
    }
    *out = val;
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

/* --- src/err/err.c --- */


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void err_set(Err *e, ErrKind kind, int code, const char *fmt, ...) {
    if (e == NULL) return;
    e->kind = kind;
    e->code = code;
    va_list ap;
    va_start(ap, fmt);
    if (vsnprintf(e->msg, sizeof e->msg, fmt, ap) < 0) e->msg[0] = '\0';
    va_end(ap);
}

static ErrKind err_kind_(int code) {
    switch (code) {
    case ENOMEM:
        return ERR_NO_MEMORY;
    case ENOENT:
        return ERR_NOT_FOUND;
    case EEXIST:
        return ERR_EXISTS;
    case EACCES:
    case EPERM:
        return ERR_PERMISSION;
    case ENOTDIR:
        return ERR_NOT_DIR;
    case EISDIR:
        return ERR_IS_DIR;
    default:
        return ERR_OS;
    }
}

void err_os(Err *e, int code, const char *op, StrView subject) {
    if (e == NULL) return;
    char buf[128];
    /* A program that defines _GNU_SOURCE gets glibc's strerror_r, which
     * returns the text instead of filling buf. */
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
    const char *reason = strerror_r(code, buf, sizeof buf);
#else
    const char *reason = strerror_r(code, buf, sizeof buf) == 0 ? buf : "";
#endif
    if (subject.len == 0)
        err_set(e, err_kind_(code), code, "%s: %s", op, reason);
    else
        err_set(e, err_kind_(code), code, "%s %.*s: %s", op, STR_FMT(subject),
                reason);
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
 * slots without hashing a key again. Removal fills the hole in `data` with
 * the last entry, and backward-shift deletion closes the gap in the index
 * without tombstones. */

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

bool map_remove_(void *data, size_t *len, uint64_t *slots, size_t slot_count,
                 size_t elem, StrView key) {
    char *d = data;
    size_t hole;
    if (slot_count == 0 ||
        !map_probe_(d, elem, slots, slot_count, key, map_hash_(key), &hole))
        return false;

    /* Repoint the last entry's slot before its bytes move, so the probe
     * cannot stop at the hole's slot. */
    size_t at = (uint32_t)slots[hole] - 1, last = *len - 1;
    if (at != last) {
        StrView k;
        size_t p;
        memcpy(&k, d + last * elem, sizeof k);
        (void)map_probe_(d, elem, slots, slot_count, k, map_hash_(k), &p);
        slots[p] = (slots[p] & ~(uint64_t)UINT32_MAX) | (uint64_t)(at + 1);
        memcpy(d + at * elem, d + last * elem, elem);
    }

    /* Pull back each later slot in the run whose home is at or before the
     * hole, so no probe chain crosses an empty slot. */
    size_t mask = slot_count - 1;
    for (size_t j = (hole + 1) & mask; slots[j] != 0; j = (j + 1) & mask) {
        size_t home = (uint32_t)(slots[j] >> 32) & mask;
        if (((j - home) & mask) >= ((j - hole) & mask)) {
            slots[hole] = slots[j];
            hole = j;
        }
    }
    slots[hole] = 0;
    *len = last;
    return true;
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

/* --- src/fs/fs.c --- */


#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

typedef Arr(size_t) FsOffsets_;
typedef Arr(const char *) FsNames_;

static void fs_oom_(Err *err, const char *op, const char *path) {
    err_set(err, ERR_NO_MEMORY, 0, "%s %s: out of memory", op, path);
}

static FsType fs_type_(mode_t m) {
    if (S_ISREG(m)) return FS_FILE;
    if (S_ISDIR(m)) return FS_DIR;
    if (S_ISLNK(m)) return FS_LINK;
    return FS_OTHER;
}

static int64_t fs_mtime_ns_(const struct stat *s) {
#if defined(__APPLE__) && defined(st_mtime)
    /* A program that defined _DARWIN_C_SOURCE gets the BSD layout. */
    return (int64_t)s->st_mtimespec.tv_sec * 1000000000 +
           s->st_mtimespec.tv_nsec;
#elif defined(__APPLE__)
    return (int64_t)s->st_mtime * 1000000000 + s->st_mtimensec;
#else
    return (int64_t)s->st_mtim.tv_sec * 1000000000 + s->st_mtim.tv_nsec;
#endif
}

static bool fs_stat_(const char *path, FsStat *st, Err *err, bool follow) {
    struct stat s;
    if ((follow ? stat(path, &s) : lstat(path, &s)) != 0) {
        err_os(err, errno, follow ? "stat" : "lstat", sv_from_cstr(path));
        return false;
    }
    *st = (FsStat){ .type = fs_type_(s.st_mode),
                    .size = (uint64_t)s.st_size,
                    .mtime_ns = fs_mtime_ns_(&s),
                    .mode = (uint32_t)(s.st_mode & 07777) };
    return true;
}

bool fs_stat(const char *path, FsStat *st, Err *err) {
    return fs_stat_(path, st, err, true);
}

bool fs_lstat(const char *path, FsStat *st, Err *err) {
    return fs_stat_(path, st, err, false);
}

bool fs_exists(const char *path) {
    struct stat s;
    return stat(path, &s) == 0;
}

bool fs_read(Str *out, const char *path, Err *err) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        err_os(err, errno, "open", sv_from_cstr(path));
        return false;
    }
    bool ok = str_append_file(out, f);
    if (!ok && ferror(f))
        err_os(err, errno, "read", sv_from_cstr(path));
    else if (!ok)
        fs_oom_(err, "read", path);
    fclose(f);
    return ok;
}

/* Leaves errno set on failure. */
static bool fs_write_all_(int fd, StrView data) {
    const char *p = data.data;
    size_t left = data.len;
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return false;
        p += n;
        left -= (size_t)n;
    }
    return true;
}

bool fs_write(const char *path, StrView data, Err *err) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    if (fd < 0) {
        err_os(err, errno, "open", sv_from_cstr(path));
        return false;
    }
    bool ok = fs_write_all_(fd, data);
    if (!ok) err_os(err, errno, "write", sv_from_cstr(path));
    if (close(fd) != 0 && ok) {
        err_os(err, errno, "close", sv_from_cstr(path));
        ok = false;
    }
    return ok;
}

bool fs_write_atomic(const char *path, StrView data, Err *err) {
    Str tmp = { 0 };
    if (!str_appendf(&tmp, "%s.tmpXXXXXX", path)) {
        fs_oom_(err, "write", path);
        return false;
    }
    struct stat s;
    mode_t mode = stat(path, &s) == 0 ? s.st_mode & 07777 : 0644;
    int fd = mkstemp(tmp.data);
    if (fd < 0) {
        err_os(err, errno, "create", str_view(&tmp));
        str_free(&tmp);
        return false;
    }
    /* fsync first, or a crash after the rename can leave an empty file. */
    bool ok = fchmod(fd, mode) == 0 &&
              fs_write_all_(fd, data) &&
              fsync(fd) == 0;
    if (!ok) err_os(err, errno, "write", str_view(&tmp));
    if (close(fd) != 0 && ok) {
        err_os(err, errno, "close", str_view(&tmp));
        ok = false;
    }
    if (ok && rename(tmp.data, path) != 0) {
        err_os(err, errno, "rename", sv_from_cstr(path));
        ok = false;
    }
    if (!ok) unlink(tmp.data);
    str_free(&tmp);
    return ok;
}

bool fs_mkdir_p(const char *path, Err *err) {
    Str p = { 0 };
    if (!str_from_cstr(&p, path)) {
        fs_oom_(err, "mkdir", path);
        return false;
    }
    while (p.len > 1 && p.data[p.len - 1] == '/') p.data[--p.len] = '\0';

    bool ok = true;
    /* Make each prefix that ends before a slash, then the whole path. */
    for (size_t i = 1; ok && i <= p.len; i++) {
        if (i < p.len && p.data[i] != '/') continue;
        if (p.data[i - 1] == '/') continue;
        char saved = p.data[i];
        p.data[i] = '\0';
        struct stat s;
        if (mkdir(p.data, 0777) != 0) {
            int code = errno;
            if (code != EEXIST) {
                err_os(err, code, "mkdir", sv_from_cstr(p.data));
                ok = false;
            } else if (stat(p.data, &s) != 0 || !S_ISDIR(s.st_mode)) {
                err_os(err, i == p.len ? EEXIST : ENOTDIR, "mkdir",
                       sv_from_cstr(p.data));
                ok = false;
            }
        }
        p.data[i] = saved;
    }
    str_free(&p);
    return ok;
}

bool fs_remove(const char *path, Err *err) {
    if (remove(path) == 0) return true;
    err_os(err, errno, "remove", sv_from_cstr(path));
    return false;
}

static int fs_name_cmp_(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* The entries of `dir`, sorted, as pointers into `buf`. */
static bool fs_list_(const char *dir, Str *buf, FsNames_ *names, Err *err) {
    DIR *d = opendir(dir);
    if (d == NULL) {
        err_os(err, errno, "open", sv_from_cstr(dir));
        return false;
    }
    FsOffsets_ offs = { 0 };
    bool ok = true;
    for (;;) {
        errno = 0;
        struct dirent *e = readdir(d);
        if (e == NULL) {
            if (errno != 0) {
                err_os(err, errno, "read", sv_from_cstr(dir));
                ok = false;
            }
            break;
        }
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        size_t off = buf->len;
        if (!(arr_push(&offs, off) &&
              str_append_cstr(buf, e->d_name) &&
              str_push(buf, '\0'))) {
            fs_oom_(err, "read", dir);
            ok = false;
            break;
        }
    }
    closedir(d);

    if (ok && !arr_reserve(names, offs.len)) {
        fs_oom_(err, "read", dir);
        ok = false;
    }
    for (size_t i = 0; ok && i < offs.len; i++)
        names->data[names->len++] = buf->data + offs.data[i];
    /* An empty directory leaves data null, which glibc's qsort rejects. */
    if (ok && names->len > 1)
        qsort(names->data, names->len, sizeof *names->data, fs_name_cmp_);
    arr_free(&offs);
    return ok;
}

/* Cut `path` back to `len` bytes after a child was joined onto it. */
static void fs_truncate_(Str *path, size_t len) {
    path->len = len;
    path->data[len] = '\0';
}

static bool fs_remove_tree_(Str *path, Err *err) {
    struct stat s;
    if (lstat(path->data, &s) != 0) {
        if (errno == ENOENT) return true;
        err_os(err, errno, "lstat", str_view(path));
        return false;
    }
    if (S_ISDIR(s.st_mode)) {
        Str buf = { 0 };
        FsNames_ names = { 0 };
        bool ok = fs_list_(path->data, &buf, &names, err);
        size_t base = path->len;
        for (size_t i = 0; ok && i < names.len; i++) {
            if (!fs_join(path, sv_from_cstr(names.data[i]))) {
                fs_oom_(err, "remove", path->data);
                ok = false;
                break;
            }
            ok = fs_remove_tree_(path, err);
            fs_truncate_(path, base);
        }
        str_free(&buf);
        arr_free(&names);
        if (!ok) return false;
    }
    if (remove(path->data) != 0) {
        err_os(err, errno, "remove", str_view(path));
        return false;
    }
    return true;
}

bool fs_remove_all(const char *path, Err *err) {
    Str p = { 0 };
    if (!str_from_cstr(&p, path)) {
        fs_oom_(err, "remove", path);
        return false;
    }
    bool ok = fs_remove_tree_(&p, err);
    str_free(&p);
    return ok;
}

bool fs_rename(const char *from, const char *to, Err *err) {
    if (rename(from, to) == 0) return true;
    err_os(err, errno, "rename", sv_from_cstr(from));
    return false;
}

bool fs_copy(const char *from, const char *to, Err *err) {
    int in = open(from, O_RDONLY | O_CLOEXEC);
    if (in < 0) {
        err_os(err, errno, "open", sv_from_cstr(from));
        return false;
    }
    struct stat s;
    if (fstat(in, &s) != 0) {
        err_os(err, errno, "stat", sv_from_cstr(from));
        close(in);
        return false;
    }
    int out = open(to, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (out < 0) {
        err_os(err, errno, "open", sv_from_cstr(to));
        close(in);
        return false;
    }

    char buf[16384];
    bool ok = true;
    for (;;) {
        ssize_t n = read(in, buf, sizeof buf);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) {
            err_os(err, errno, "read", sv_from_cstr(from));
            ok = false;
        } else if (n > 0 && !fs_write_all_(out, (StrView){ buf, (size_t)n })) {
            err_os(err, errno, "write", sv_from_cstr(to));
            ok = false;
        }
        if (n <= 0 || !ok) break;
    }
    if (ok && fchmod(out, s.st_mode & 07777) != 0) {
        err_os(err, errno, "chmod", sv_from_cstr(to));
        ok = false;
    }
    if (close(out) != 0 && ok) {
        err_os(err, errno, "close", sv_from_cstr(to));
        ok = false;
    }
    close(in);
    return ok;
}

/* Append the target of the link at `path` to `out`, unchanged on failure. */
static bool fs_readlink_(const char *path, Str *out, Err *err) {
    Str tmp = { .alloc = out->alloc };
    for (size_t want = 256;; want *= 2) {
        if (!str_reserve(&tmp, want)) {
            fs_oom_(err, "readlink", path);
            str_free(&tmp);
            return false;
        }
        ssize_t n = readlink(path, tmp.data, tmp.capacity);
        if (n < 0) {
            err_os(err, errno, "readlink", sv_from_cstr(path));
            str_free(&tmp);
            return false;
        }
        if ((size_t)n < tmp.capacity) {
            tmp.len = (size_t)n;
            tmp.data[n] = '\0';
            break;
        }
    }
    bool ok = str_append(out, str_view(&tmp));
    if (!ok) fs_oom_(err, "readlink", path);
    str_free(&tmp);
    return ok;
}

static bool fs_copy_tree_(Str *from, Str *to, Err *err) {
    struct stat s;
    if (lstat(from->data, &s) != 0) {
        err_os(err, errno, "lstat", str_view(from));
        return false;
    }
    if (S_ISREG(s.st_mode)) return fs_copy(from->data, to->data, err);
    if (S_ISLNK(s.st_mode)) {
        Str target = { 0 };
        bool ok = fs_readlink_(from->data, &target, err);
        if (ok && symlink(str_cstr(&target), to->data) != 0) {
            err_os(err, errno, "symlink", str_view(to));
            ok = false;
        }
        str_free(&target);
        return ok;
    }
    if (!S_ISDIR(s.st_mode)) {
        err_set(err, ERR_INVALID, 0, "copy %s: not a file, directory or link",
                from->data);
        return false;
    }

    if (mkdir(to->data, 0700) != 0 && errno != EEXIST) {
        err_os(err, errno, "mkdir", str_view(to));
        return false;
    }
    Str buf = { 0 };
    FsNames_ names = { 0 };
    bool ok = fs_list_(from->data, &buf, &names, err);
    size_t from_len = from->len, to_len = to->len;
    for (size_t i = 0; ok && i < names.len; i++) {
        StrView name = sv_from_cstr(names.data[i]);
        if (!(fs_join(from, name) && fs_join(to, name))) {
            fs_oom_(err, "copy", from->data);
            ok = false;
        } else {
            ok = fs_copy_tree_(from, to, err);
        }
        fs_truncate_(from, from_len);
        fs_truncate_(to, to_len);
    }
    str_free(&buf);
    arr_free(&names);
    /* Set the mode last, so a read-only directory can still be filled. */
    if (ok && chmod(to->data, s.st_mode & 07777) != 0) {
        err_os(err, errno, "chmod", str_view(to));
        ok = false;
    }
    return ok;
}

bool fs_copy_tree(const char *from, const char *to, Err *err) {
    Str f = { 0 }, t = { 0 };
    bool ok = str_from_cstr(&f, from) && str_from_cstr(&t, to);
    if (!ok)
        fs_oom_(err, "copy", from);
    else
        ok = fs_copy_tree_(&f, &t, err);
    str_free(&f);
    str_free(&t);
    return ok;
}

static bool fs_walk_dir_(Str *path, FsWalkFn *fn, void *ctx, Err *err,
                         bool *stop) {
    Str buf = { 0 };
    FsNames_ names = { 0 };
    bool ok = fs_list_(path->data, &buf, &names, err);
    size_t base = path->len;
    for (size_t i = 0; ok && !*stop && i < names.len; i++) {
        struct stat s;
        if (!fs_join(path, sv_from_cstr(names.data[i]))) {
            fs_oom_(err, "walk", path->data);
            ok = false;
        } else if (lstat(path->data, &s) != 0) {
            err_os(err, errno, "lstat", str_view(path));
            ok = false;
        } else {
            FsType type = fs_type_(s.st_mode);
            FsWalkAct act = fn(ctx, path->data, type);
            if (act == FS_WALK_STOP)
                *stop = true;
            else if (act == FS_WALK_NEXT && type == FS_DIR)
                ok = fs_walk_dir_(path, fn, ctx, err, stop);
        }
        fs_truncate_(path, base);
    }
    str_free(&buf);
    arr_free(&names);
    return ok;
}

bool fs_walk(const char *root, FsWalkFn *fn, void *ctx, Err *err) {
    struct stat s;
    if (lstat(root, &s) != 0) {
        err_os(err, errno, "lstat", sv_from_cstr(root));
        return false;
    }
    FsType type = fs_type_(s.st_mode);
    if (fn(ctx, root, type) != FS_WALK_NEXT || type != FS_DIR) return true;

    Str path = { 0 };
    if (!str_from_cstr(&path, root)) {
        fs_oom_(err, "walk", root);
        return false;
    }
    bool stop = false;
    bool ok = fs_walk_dir_(&path, fn, ctx, err, &stop);
    str_free(&path);
    return ok;
}

bool fs_exe_path(Str *out, Err *err) {
#ifdef __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(NULL, &size); /* fails, and sets the size needed */
    Str raw = { 0 };
    if (!str_reserve(&raw, size)) {
        fs_oom_(err, "find", "the executable");
        return false;
    }
    char real[PATH_MAX];
    bool ok = _NSGetExecutablePath(raw.data, &size) == 0 &&
              realpath(raw.data, real) != NULL;
    if (!ok) err_os(err, errno, "find", SV("the executable"));
    str_free(&raw);
    if (ok && !str_append_cstr(out, real)) {
        fs_oom_(err, "find", "the executable");
        ok = false;
    }
    return ok;
#else
    return fs_readlink_("/proc/self/exe", out, err);
#endif
}

static StrView fs_strip_slashes_(StrView path) {
    while (path.len > 1 && path.data[path.len - 1] == '/') path.len--;
    return path;
}

StrView fs_basename(StrView path) {
    path = fs_strip_slashes_(path);
    size_t at = sv_find_last(path, SV("/"));
    if (at == SV_NPOS || path.len == 1) return path;
    return sv_slice(path, at + 1, path.len);
}

StrView fs_dirname(StrView path) {
    path = fs_strip_slashes_(path);
    size_t at = sv_find_last(path, SV("/"));
    if (at == SV_NPOS) return SV(".");
    if (at == 0) return SV("/");
    return fs_strip_slashes_(sv_slice(path, 0, at));
}

StrView fs_extension(StrView path) {
    StrView base = fs_basename(path);
    size_t at = sv_find_last(base, SV("."));
    if (at == SV_NPOS || at == 0 || str_eq(base, SV("..")))
        return sv_slice(base, base.len, base.len);
    return sv_slice(base, at, base.len);
}

bool fs_join(Str *path, StrView part) {
    if (path->len > 0)
        while (part.len > 0 && part.data[0] == '/')
            part = sv_slice(part, 1, part.len);
    if (part.len == 0) return true;
    bool slash = path->len > 0 && path->data[path->len - 1] != '/';
    if (!str_reserve(path, part.len + slash)) return false;
    return (!slash || str_push(path, '/')) && str_append(path, part);
}

/* --- src/args/args.c --- */


#include <string.h>

static const ArgOpt *args_find_long_(const Args *a, StrView name) {
    for (size_t i = 0; i < a->opt_count; i++) {
        const char *n = a->opts[i].name;
        if (n != NULL && str_eq(sv_from_cstr(n), name)) return &a->opts[i];
    }
    return NULL;
}

static const ArgOpt *args_find_short_(const Args *a, char c) {
    for (size_t i = 0; i < a->opt_count; i++) {
        if (a->opts[i].short_name == c) return &a->opts[i];
    }
    return NULL;
}

/* Check `value` for `o`, and store it when `apply` is set. */
static bool args_set_(const ArgOpt *o, const char *value, bool apply,
                      Err *err) {
    uint64_t n = 0;
    if (o->kind == ARG_U64 && !sv_parse_u64(sv_from_cstr(value), 10, &n)) {
        err_set(err, ERR_INVALID, 0, "%s%s: expected a number, got '%s'",
                o->name != NULL ? "--" : "-",
                o->name != NULL ? o->name : (char[]){ o->short_name, '\0' },
                value);
        return false;
    }
    if (!apply) return true;
    switch (o->kind) {
    case ARG_FLAG:
        *(bool *)o->out = true;
        break;
    case ARG_STR:
        *(const char **)o->out = value;
        break;
    case ARG_U64:
        *(uint64_t *)o->out = n;
        break;
    }
    return true;
}

/* One pass over argv. The first only checks and counts positionals, the
 * second stores, so a failed parse changes nothing. */
static bool args_pass_(Args *a, int argc, char **argv, bool apply,
                       size_t *positionals, Err *err) {
    bool options = true, have_cmd = a->cmd != NULL;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (options && strcmp(arg, "--") == 0) {
            options = false;
        } else if (options && arg[0] == '-' && arg[1] == '-') {
            StrView name = sv_from_cstr(arg + 2);
            size_t eq = sv_find_byte(name, '=');
            if (eq != SV_NPOS) name = sv_slice(name, 0, eq);
            const ArgOpt *o = args_find_long_(a, name);
            if (o == NULL && str_eq(name, SV("help")) && eq == SV_NPOS) {
                if (apply) a->help = true;
                continue;
            }
            if (o == NULL) {
                err_set(err, ERR_INVALID, 0, "unknown option '%s'", arg);
                return false;
            }
            const char *value = eq != SV_NPOS ? arg + 2 + eq + 1 : NULL;
            if (o->kind == ARG_FLAG && value != NULL) {
                err_set(err, ERR_INVALID, 0, "--%s takes no value", o->name);
                return false;
            }
            if (o->kind != ARG_FLAG && value == NULL) {
                if (i + 1 == argc) {
                    err_set(err, ERR_INVALID, 0, "--%s needs a value", o->name);
                    return false;
                }
                value = argv[++i];
            }
            if (!args_set_(o, value, apply, err)) return false;
        } else if (options && arg[0] == '-' && arg[1] != '\0') {
            /* A group of short options, the last of which may take a value. */
            for (const char *c = arg + 1; *c != '\0'; c++) {
                const ArgOpt *o = args_find_short_(a, *c);
                if (o == NULL && *c == 'h') {
                    if (apply) a->help = true;
                    continue;
                }
                if (o == NULL) {
                    err_set(err, ERR_INVALID, 0, "unknown option '-%c' in '%s'",
                            *c, arg);
                    return false;
                }
                if (o->kind == ARG_FLAG) {
                    if (!args_set_(o, NULL, apply, err)) return false;
                    continue;
                }
                const char *value = c[1] != '\0' ? c + 1 : NULL;
                if (value == NULL && i + 1 == argc) {
                    err_set(err, ERR_INVALID, 0, "-%c needs a value", *c);
                    return false;
                }
                if (value == NULL) value = argv[++i];
                if (!args_set_(o, value, apply, err)) return false;
                break;
            }
        } else if (a->cmd_count > 0 && !have_cmd) {
            size_t k = 0;
            while (k < a->cmd_count && strcmp(a->cmds[k].name, arg) != 0) k++;
            if (k == a->cmd_count) {
                err_set(err, ERR_INVALID, 0, "unknown command '%s'", arg);
                return false;
            }
            have_cmd = true;
            if (apply) a->cmd = a->cmds[k].name;
        } else if (apply) {
            a->rest.data[a->rest.len++] = arg;
        } else {
            (*positionals)++;
        }
    }
    return true;
}

bool args_parse(Args *a, int argc, char **argv, Err *err) {
    size_t positionals = 0;
    if (!args_pass_(a, argc, argv, false, &positionals, err)) return false;
    if (!arr_reserve(&a->rest, positionals)) {
        err_set(err, ERR_NO_MEMORY, 0, "parse arguments: out of memory");
        return false;
    }
    return args_pass_(a, argc, argv, true, NULL, err);
}

/* The left column of an option's help line, such as "-j, --jobs=N". */
static bool args_opt_left_(Str *s, const ArgOpt *o) {
    const char *value = o->value_name != NULL ? o->value_name : "VALUE";
    bool ok = o->short_name != 0 ? str_appendf(s, "-%c", o->short_name)
                                 : str_append_cstr(s, "  ");
    if (ok && o->name != NULL)
        ok = str_appendf(s, "%s--%s", o->short_name != 0 ? ", " : "  ",
                         o->name);
    if (ok && o->kind != ARG_FLAG)
        ok = o->name != NULL ? str_appendf(s, "=%s", value)
                             : str_appendf(s, " %s", value);
    return ok;
}

bool args_help(Str *out, const Args *a) {
    Str s = { .alloc = out->alloc }, left = { .alloc = out->alloc };
    bool ok = str_appendf(&s, "usage: %s\n", a->usage != NULL ? a->usage : "");

    /* One column width for commands and options alike. */
    size_t width = sizeof "-h, --help" - 1;
    for (size_t i = 0; ok && i < a->cmd_count; i++) {
        size_t n = strlen(a->cmds[i].name);
        if (n > width) width = n;
    }
    for (size_t i = 0; ok && i < a->opt_count; i++) {
        str_clear(&left);
        ok = args_opt_left_(&left, &a->opts[i]);
        if (left.len > width) width = left.len;
    }

    if (ok && a->cmd_count > 0) ok = str_append_cstr(&s, "\ncommands:\n");
    for (size_t i = 0; ok && i < a->cmd_count; i++) {
        const ArgCmd *c = &a->cmds[i];
        ok = str_appendf(&s, "  %-*s  %s\n", (int)width, c->name,
                         c->help != NULL ? c->help : "");
    }
    if (ok) ok = str_append_cstr(&s, "\noptions:\n");
    for (size_t i = 0; ok && i < a->opt_count; i++) {
        str_clear(&left);
        ok = args_opt_left_(&left, &a->opts[i]) &&
             str_appendf(&s, "  %-*s  %s\n", (int)width, str_cstr(&left),
                         a->opts[i].help != NULL ? a->opts[i].help : "");
    }
    bool builtin_help = args_find_long_(a, SV("help")) == NULL &&
                        args_find_short_(a, 'h') == NULL;
    if (ok && builtin_help)
        ok = str_appendf(&s, "  %-*s  %s\n", (int)width, "-h, --help",
                         "show this help");
    ok = ok && str_append(out, str_view(&s));
    str_free(&s);
    str_free(&left);
    return ok;
}

void args_free(Args *a) {
    arr_free(&a->rest);
}

/* --- src/proc/proc.c --- */


#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
/* <sys/sysctl.h> does not compile under _POSIX_C_SOURCE on macOS. */
int sysctlbyname(const char *name, void *oldp, size_t *oldlenp, void *newp,
                 size_t newlen);
#endif

bool cmd_append_(Cmd *c, const char *const *args, size_t n) {
    if (!arr_reserve(c, n)) return false;
    memcpy(c->data + c->len, args, n * sizeof *args);
    c->len += n;
    return true;
}

/* True when the shell reads `a` as itself without quotes. */
static bool proc_plain_(const char *a) {
    if (*a == '\0') return false;
    for (; *a != '\0'; a++) {
        char ch = *a;
        bool alnum = (ch >= 'a' && ch <= 'z') ||
                     (ch >= 'A' && ch <= 'Z') ||
                     (ch >= '0' && ch <= '9');
        if (!alnum && strchr("@%+=:,./_-", ch) == NULL) return false;
    }
    return true;
}

bool cmd_render(Str *out, const Cmd *c) {
    Str s = { .alloc = out->alloc };
    bool ok = true;
    for (size_t i = 0; ok && i < c->len; i++) {
        const char *a = c->data[i];
        if (i > 0) ok = str_push(&s, ' ');
        if (ok && proc_plain_(a)) {
            ok = str_append_cstr(&s, a);
            continue;
        }
        /* Single quotes keep everything but a quote, which closes the
         * string, adds an escaped quote and reopens it. */
        ok = ok && str_push(&s, '\'');
        for (; ok && *a != '\0'; a++)
            ok = *a == '\'' ? str_append_cstr(&s, "'\\''") : str_push(&s, *a);
        ok = ok && str_push(&s, '\'');
    }
    ok = ok && str_append(out, str_view(&s));
    str_free(&s);
    return ok;
}

static bool proc_open_(int *fd, const char *path, int flags, Err *err) {
    *fd = open(path, flags | O_CLOEXEC, 0666);
    if (*fd >= 0) return true;
    err_os(err, errno, "open", sv_from_cstr(path));
    return false;
}

static void proc_close_(int fd) {
    if (fd >= 0) close(fd);
}

/* proc_start, with stdout on `out_fd` instead when it is not -1. */
static bool proc_spawn_(Proc *p, const Cmd *c, const CmdIo *io, int out_fd,
                        Err *err) {
    if (c->len == 0) {
        err_set(err, ERR_INVALID, 0, "run: empty command");
        return false;
    }
    const CmdIo none = { 0 };
    if (io == NULL) io = &none;
    const int wr = O_WRONLY | O_CREAT | O_TRUNC;
    int in = -1, out = -1, errfd = -1;
    bool ok = (io->in == NULL || proc_open_(&in, io->in, O_RDONLY, err)) &&
              (out_fd >= 0 ||
               io->out == NULL ||
               proc_open_(&out, io->out, wr, err));
    if (ok && io->err != NULL) {
        /* Two opens of one file would each write from offset 0. */
        if (out >= 0 && strcmp(io->err, io->out) == 0)
            errfd = out;
        else
            ok = proc_open_(&errfd, io->err, wr, err);
    }
    int child_out = out_fd >= 0 ? out_fd : out;

    size_t argv_size = (c->len + 1) * sizeof(const char *);
    const char **argv = NULL;
    if (ok && (argv = mem_realloc(NULL, NULL, 0, argv_size)) == NULL) {
        err_set(err, ERR_NO_MEMORY, 0, "run %s: out of memory", c->data[0]);
        ok = false;
    }
    /* The child writes errno here if exec fails. A successful exec closes
     * it, so the parent reads nothing. */
    int status_pipe[2] = { -1, -1 };
    if (ok && pipe(status_pipe) != 0) {
        err_os(err, errno, "pipe", SV(""));
        ok = false;
    }
    pid_t pid = -1;
    if (ok) {
        fcntl(status_pipe[0], F_SETFD, FD_CLOEXEC);
        fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC);
        memcpy(argv, c->data, c->len * sizeof *argv);
        argv[c->len] = NULL;
        fflush(stdout); /* or its buffered text prints after the child's */
        pid = fork();
        if (pid < 0) {
            err_os(err, errno, "fork", SV(""));
            ok = false;
        }
    }
    if (pid == 0) {
        /* Only async-signal-safe calls between fork and exec. */
        if ((in < 0 || dup2(in, STDIN_FILENO) >= 0) &&
            (child_out < 0 || dup2(child_out, STDOUT_FILENO) >= 0) &&
            (errfd < 0 || dup2(errfd, STDERR_FILENO) >= 0))
            /* exec takes char *const[] for historical reasons only. */
            execvp(argv[0], (char *const *)(uintptr_t)argv);
        int code = errno;
        ssize_t n = write(status_pipe[1], &code, sizeof code);
        (void)n;
        _exit(127);
    }

    proc_close_(status_pipe[1]);
    if (ok) {
        int code = 0;
        ssize_t n;
        do n = read(status_pipe[0], &code, sizeof code);
        while (n < 0 && errno == EINTR);
        if (n == (ssize_t)sizeof code) {
            int status;
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            }
            err_os(err, code, "run", sv_from_cstr(c->data[0]));
            ok = false;
        }
    }
    proc_close_(status_pipe[0]);
    proc_close_(in);
    proc_close_(out);
    if (errfd != out) proc_close_(errfd);
    argv = mem_realloc(NULL, argv, argv_size, 0);
    if (ok) p->id_ = pid;
    return ok;
}

bool proc_start(Proc *p, const Cmd *c, const CmdIo *io, Err *err) {
    return proc_spawn_(p, c, io, -1, err);
}

/* proc_wait, naming the process `name` in the message when it is known. */
static bool proc_wait_(Proc *p, const char *name, Err *err) {
    if (p->id_ == 0) {
        err_set(err, ERR_INVALID, 0, "wait: no process");
        return false;
    }
    pid_t pid = (pid_t)p->id_;
    p->id_ = 0;
    int status = 0;
    pid_t r;
    do r = waitpid(pid, &status, 0);
    while (r < 0 && errno == EINTR);
    if (r < 0) {
        err_os(err, errno, "wait", SV(""));
        return false;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return true;

    char who[32];
    if (name == NULL) {
        snprintf(who, sizeof who, "process %ld", (long)pid);
        name = who;
    }
    if (WIFEXITED(status))
        err_set(err, ERR_EXIT, WEXITSTATUS(status), "%s exited with code %d",
                name, WEXITSTATUS(status));
    else
        err_set(err, ERR_SIGNAL, WTERMSIG(status), "%s was killed by signal %d",
                name, WTERMSIG(status));
    return false;
}

bool proc_wait(Proc *p, Err *err) {
    return proc_wait_(p, NULL, err);
}

bool cmd_run(const Cmd *c, Err *err) {
    Proc p = { 0 };
    return proc_start(&p, c, NULL, err) && proc_wait_(&p, c->data[0], err);
}

bool cmd_capture(const Cmd *c, Str *out, Err *err) {
    int fds[2];
    if (pipe(fds) != 0) {
        err_os(err, errno, "pipe", SV(""));
        return false;
    }
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    Proc p = { 0 };
    bool started = proc_spawn_(&p, c, NULL, fds[1], err);
    close(fds[1]); /* or the read below never sees end of file */
    if (!started) {
        close(fds[0]);
        return false;
    }

    /* Read everything before waiting, or a full pipe blocks the child. */
    Str tmp = { .alloc = out->alloc };
    FILE *f = fdopen(fds[0], "rb");
    bool read_ok = f != NULL && str_append_file(&tmp, f);
    if (!read_ok && (f == NULL || ferror(f)))
        err_os(err, errno, "read output of", sv_from_cstr(c->data[0]));
    else if (!read_ok)
        err_set(err, ERR_NO_MEMORY, 0, "run %s: out of memory", c->data[0]);
    if (f != NULL)
        fclose(f);
    else
        close(fds[0]);

    bool ok = proc_wait_(&p, c->data[0], read_ok ? err : NULL) && read_ok;
    if (ok && !str_append(out, str_view(&tmp))) {
        err_set(err, ERR_NO_MEMORY, 0, "run %s: out of memory", c->data[0]);
        ok = false;
    }
    str_free(&tmp);
    return ok;
}

static size_t proc_cpu_count_(void) {
#ifdef __APPLE__
    int n = 0;
    size_t len = sizeof n;
    if (sysctlbyname("hw.ncpu", &n, &len, NULL, 0) != 0 || n < 1) return 1;
    return (size_t)n;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (size_t)n : 1;
#endif
}

bool proc_pool_start(ProcPool *pool, const Cmd *c, Err *err) {
    size_t max = pool->max != 0 ? pool->max : proc_cpu_count_();
    while (pool->running.len >= max) {
        ProcJob oldest = pool->running.data[0];
        arr_remove(&pool->running, 0);
        if (!proc_wait_(&oldest.proc, oldest.name, err)) return false;
    }
    if (!arr_reserve(&pool->running, 1)) {
        err_set(err, ERR_NO_MEMORY, 0, "run %s: out of memory", c->data[0]);
        return false;
    }
    Proc p = { 0 };
    if (!proc_start(&p, c, NULL, err)) return false;
    pool->running.data[pool->running.len++] = (ProcJob){ p, c->data[0] };
    return true;
}

bool proc_pool_wait(ProcPool *pool, Err *err) {
    bool ok = true;
    for (size_t i = 0; i < pool->running.len; i++) {
        Err e = { 0 };
        ProcJob *job = &pool->running.data[i];
        if (!proc_wait_(&job->proc, job->name, &e) && ok) {
            ok = false;
            if (err != NULL) *err = e;
        }
    }
    arr_clear(&pool->running);
    return ok;
}

void proc_pool_free(ProcPool *pool) {
    if (!proc_pool_wait(pool, NULL)) {
        /* How the jobs ended does not matter once the pool is going. */
    }
    arr_free(&pool->running);
}

/* --- src/build/build.c --- */


#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool build_needs_rebuild(const char *out, const char *const *inputs, size_t n,
                         bool *needed, Err *err) {
    int64_t newest = INT64_MIN;
    for (size_t i = 0; i < n; i++) {
        FsStat st = { 0 };
        if (!fs_stat(inputs[i], &st, err)) return false;
        if (st.mtime_ns > newest) newest = st.mtime_ns;
    }
    FsStat o = { 0 };
    Err e = { 0 };
    if (!fs_stat(out, &o, &e) && e.kind != ERR_NOT_FOUND) {
        if (err != NULL) *err = e;
        return false;
    }
    *needed = o.type == FS_MISSING || newest > o.mtime_ns;
    return true;
}

/* Rewrite `d->text` in place into NUL-terminated names, and point `d->paths`
 * at each. The writer never passes the reader, so one buffer serves both. */
static bool build_parse_deps_(Deps *d) {
    char *r = d->text.data, *end = r + d->text.len, *w = r;
    /* Skip the target: everything up to a colon followed by a space or an
     * end of line. */
    while (r < end &&
           !(r[0] == ':' &&
             (r + 1 == end || r[1] == ' ' || r[1] == '\t' || r[1] == '\n')))
        r += r[0] == '\\' && r + 1 < end ? 2 : 1;
    if (r < end) r++;

    for (;;) {
        while (r < end && (*r == ' ' ||
                           *r == '\t' ||
                           (r[0] == '\\' && r + 1 < end && r[1] == '\n')))
            r += *r == '\\' ? 2 : 1;
        if (r == end || *r == '\n' || *r == '\r') break;

        char *name = w;
        while (r < end && *r != ' ' && *r != '\t' && *r != '\n' && *r != '\r') {
            if (r[0] == '\\' && r + 1 < end && r[1] == '\n') break;
            if (r[0] == '\\' && r + 1 < end && (r[1] == ' ' || r[1] == '#'))
                r++;
            else if (r[0] == '$' && r + 1 < end && r[1] == '$')
                r++;
            *w++ = *r++;
        }
        *w++ = '\0';
        if (!arr_push(&d->paths, name)) return false;
    }
    return true;
}

bool build_read_deps(Deps *d, const char *depfile, Err *err) {
    Deps next = { .text = { .alloc = d->text.alloc },
                  .paths = { .alloc = d->paths.alloc } };
    if (!fs_read(&next.text, depfile, err)) return false;
    /* An empty file still needs a buffer to point into. */
    if (!str_reserve(&next.text, 1) || !build_parse_deps_(&next)) {
        err_set(err, ERR_NO_MEMORY, 0, "read %s: out of memory", depfile);
        build_deps_free(&next);
        return false;
    }
    build_deps_free(d);
    *d = next;
    return true;
}

void build_deps_free(Deps *d) {
    str_free(&d->text);
    arr_free(&d->paths);
}

bool build_needs_rebuild_deps(const char *out, const char *depfile,
                              bool *needed, Err *err) {
    FsStat o = { 0 };
    Err e = { 0 };
    if (!fs_stat(out, &o, &e)) {
        if (e.kind != ERR_NOT_FOUND) {
            if (err != NULL) *err = e;
            return false;
        }
        *needed = true;
        return true;
    }
    Deps d = { 0 };
    if (!build_read_deps(&d, depfile, &e)) {
        if (e.kind != ERR_NOT_FOUND) {
            if (err != NULL) *err = e;
            return false;
        }
        *needed = true;
        return true;
    }
    bool stale = false, ok = true;
    for (size_t i = 0; ok && !stale && i < d.paths.len; i++) {
        FsStat st = { 0 };
        e = (Err){ 0 };
        if (fs_stat(d.paths.data[i], &st, &e))
            stale = st.mtime_ns > o.mtime_ns;
        else if (e.kind == ERR_NOT_FOUND)
            stale = true;
        else
            ok = false;
    }
    build_deps_free(&d);
    if (!ok) {
        if (err != NULL) *err = e;
        return false;
    }
    *needed = stale;
    return true;
}

bool build_run(const Cmd *c, Err *err) {
    Str line = { 0 };
    if (cmd_render(&line, c)) log_info("%s", str_cstr(&line));
    str_free(&line);
    return cmd_run(c, err);
}

bool build_start(ProcPool *pool, const Cmd *c, Err *err) {
    Str line = { 0 };
    if (cmd_render(&line, c)) log_info("%s", str_cstr(&line));
    str_free(&line);
    return proc_pool_start(pool, c, err);
}

void build_rebuild_self(int argc, char **argv, const char *source,
                        const Cmd *flags) {
    (void)argc;
    /* Run from another directory, the source cannot be found to rebuild. */
    if (!fs_exists(source)) return;

    Str exe = { 0 }, dir = { 0 }, dep = { 0 }, old = { 0 };
    Cmd cc = { 0 };
    Err err = { 0 };
    bool needed = false;
    if (!fs_exe_path(&exe, &err)) log_fatal("%s", err.msg);
    StrView name = fs_basename(str_view(&exe));
    bool ok = fs_join(&dir, fs_dirname(str_view(&exe))) &&
              fs_join(&dir, SV(".build")) &&
              fs_join(&dep, str_view(&dir)) &&
              fs_join(&dep, name) &&
              str_append_cstr(&dep, ".d") &&
              fs_join(&old, str_view(&dir)) &&
              fs_join(&old, name) &&
              str_append_cstr(&old, ".old");
    if (!ok) log_fatal("rebuild %s: out of memory", source);
    if (!build_needs_rebuild_deps(str_cstr(&exe), str_cstr(&dep), &needed,
                                  &err))
        log_fatal("%s", err.msg);

    if (needed) {
        const char *compiler = getenv("CC");
        if (compiler == NULL || compiler[0] == '\0') compiler = "cc";
        ok = cmd_append(&cc, compiler, "-Wall", "-Wextra", "-MMD", "-MF",
                        str_cstr(&dep), "-o", str_cstr(&exe));
        for (size_t i = 0; ok && flags != NULL && i < flags->len; i++)
            ok = cmd_append(&cc, flags->data[i]);
        ok = ok && cmd_append(&cc, source);
        if (!ok) log_fatal("rebuild %s: out of memory", source);

        if (!fs_mkdir_p(str_cstr(&dir), &err) ||
            !fs_rename(str_cstr(&exe), str_cstr(&old), &err))
            log_fatal("%s", err.msg);
        if (!build_run(&cc, &err)) {
            log_error("%s", err.msg);
            Err back = { 0 };
            if (!fs_rename(str_cstr(&old), str_cstr(&exe), &back))
                log_error("%s", back.msg);
            exit(EXIT_FAILURE);
        }
        execv(str_cstr(&exe), argv);
        log_fatal("run %s: %s", str_cstr(&exe), strerror(errno));
    }
    str_free(&exe);
    str_free(&dir);
    str_free(&dep);
    str_free(&old);
    arr_free(&cc);
}

/* Append `s` to `out` as a JSON string, quotes included. */
static bool build_json_str_(Str *out, const char *s) {
    bool ok = str_push(out, '"');
    for (; ok && *s != '\0'; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\')
            ok = str_push(out, '\\') && str_push(out, (char)ch);
        else if (ch < 0x20)
            ok = str_appendf(out, "\\u%04x", ch);
        else
            ok = str_push(out, (char)ch);
    }
    return ok && str_push(out, '"');
}

bool build_compdb_add(CompDb *db, const char *file, const Cmd *c) {
    size_t len = db->json.len;
    bool ok = str_append_cstr(&db->json, "\"file\": ") &&
              build_json_str_(&db->json, file) &&
              str_append_cstr(&db->json, ", \"arguments\": [");
    for (size_t i = 0; ok && i < c->len; i++)
        ok = (i == 0 || str_append_cstr(&db->json, ", ")) &&
             build_json_str_(&db->json, c->data[i]);
    ok = ok && str_append_cstr(&db->json, "]\n");
    if (!ok && db->json.data != NULL) {
        db->json.len = len;
        db->json.data[len] = '\0';
    }
    return ok;
}

bool build_compdb_write(const CompDb *db, const char *path, Err *err) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof cwd) == NULL) {
        err_os(err, errno, "getcwd", SV(""));
        return false;
    }
    Str out = { 0 }, dir = { 0 }, old = { 0 };
    bool ok = build_json_str_(&dir, cwd) && str_push(&out, '[');
    StrView rest = str_view(&db->json), entry;
    bool first = true;
    while (ok && sv_split_next(&rest, SV("\n"), &entry)) {
        ok = str_append_cstr(&out, first ? "\n" : ",\n") &&
             str_append_cstr(&out, "  {\"directory\": ") &&
             str_append(&out, str_view(&dir)) &&
             str_append_cstr(&out, ", ") &&
             str_append(&out, entry) &&
             str_push(&out, '}');
        first = false;
    }
    ok = ok && str_append_cstr(&out, "\n]\n");
    if (!ok) err_set(err, ERR_NO_MEMORY, 0, "write %s: out of memory", path);

    Err e = { 0 };
    bool same = ok &&
                fs_read(&old, path, &e) &&
                str_eq(str_view(&old), str_view(&out));
    if (ok && !same) ok = fs_write_atomic(path, str_view(&out), err);
    str_free(&out);
    str_free(&dir);
    str_free(&old);
    return ok;
}

void build_compdb_free(CompDb *db) {
    str_free(&db->json);
}

#endif /* MULE_IMPLEMENTATION */
