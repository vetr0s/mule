#pragma once
/**
 * @file
 * @brief Mem: the allocator interface and an arena.
 *
 *   - An Allocator is one realloc-shaped function plus a context pointer.
 *     Modules that allocate take a `const Allocator *`, and null means the
 *     libc heap.
 *   - An Arena hands out memory from large blocks and frees it all at once.
 *     A zero-initialised Arena (`Arena a = { 0 };`) is valid and owns nothing.
 *   - Growing the most recent arena allocation happens in place when the
 *     block has room, so a Str growing in an arena does not leave a copy
 *     behind at every doubling.
 *
 * @snippet{trimleft} test_mem.c arena
 */

#include "base/base.h"

/* Callers pass alignof(T) to arena_alloc. */
#include <stdalign.h>

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
MULE_NODISCARD void *mem_realloc(const Allocator *a, void *old, size_t old_size,
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
MULE_NODISCARD void *arena_alloc(Arena *a, size_t size, size_t align);
/** An Allocator that draws from @p a. It lives as long as @p a.
 *  @snippet{trimleft} test_mem.c allocator */
MULE_NODISCARD const Allocator *arena_allocator(Arena *a);
/** Forget every allocation and keep the newest block for reuse. */
void arena_reset(Arena *a);
/** Release every block. Safe to call twice, and @p a stays usable. */
void arena_free(Arena *a);
