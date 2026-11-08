#pragma once
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

#include "base/base.h"

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
