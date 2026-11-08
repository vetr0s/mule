#include "mem/mem.h"

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
