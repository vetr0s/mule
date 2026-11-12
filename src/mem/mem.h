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
 *   - Under AddressSanitizer, an arena poisons the bytes it has not handed
 *     out and leaves 16 poisoned bytes after each allocation. Reading or
 *     writing past an allocation is then reported, as it is for malloc.
 *     Allocations also start on 8-byte boundaries.
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
