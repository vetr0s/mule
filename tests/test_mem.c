/* Tests for src/mem/mem.c.
 *
 * Bracketed names in comment pairs mark regions that src/mem/mem.h pulls into
 * the Doxygen reference with @snippet. See tests/test_str.c for the rules.
 */
#include "test/test.h"

#include "log/log.c"
#include "mem/mem.c"
#include "str/str.c"

#include <stdint.h>

#ifndef __has_feature
#define __has_feature(x) 0
#endif

/* ASan replaces libmalloc, so footprint tests need a build without it. */
#if defined(__APPLE__) && !__has_feature(address_sanitizer)
#define MEM_FOOTPRINT_TESTS 1
#include <mach/mach.h>
#else
#define MEM_FOOTPRINT_TESTS 0
#endif

/* --- Heap ---------------------------------------------------------------- */

TEST(heap_realloc_and_free) {
    char *p = mem_realloc(NULL, NULL, 0, 8);
    REQUIRE(p != NULL);
    memcpy(p, "abcdefg", 8);
    p = mem_realloc(NULL, p, 8, 4096);
    REQUIRE(p != NULL);
    CHECK_EQ_STR(p, "abcdefg");
    memset(p + 8, 'x', 4096 - 8);
    p = mem_realloc(NULL, p, 4096, 4);
    REQUIRE(p != NULL);
    CHECK_EQ_MEM(p, 4, "abcd", 4);
    CHECK(mem_realloc(NULL, p, 4, 0) == NULL);
}

/* Other tests run on the harness heap, so this one reaches libc in every build.
 * A REQUIRE in the body returns here, so the harness always comes back. */
TEST(libc_realloc_and_free) {
    const Allocator *harness = mem_set_heap(NULL);
    heap_realloc_and_free();
    mem_set_heap(harness);
}

/* --- Arena --------------------------------------------------------------- */

#if MEM_FOOTPRINT_TESTS
/* Bytes of memory the kernel charges to this process right now. */
static size_t footprint(void) {
    task_vm_info_data_t info;
    mach_msg_type_number_t n = TASK_VM_INFO_COUNT;
    kern_return_t err = task_info(mach_task_self(), TASK_VM_INFO,
                                  (task_info_t)&info, &n);
    log_assert(err == KERN_SUCCESS);
    return (size_t)info.phys_footprint;
}

/* macOS 26 libmalloc kept every large block grown by realloc dirty after
 * free, so a buffer grown and freed in a loop leaked its size each pass. */
TEST(heap_growth_returns_memory) {
    enum { SIZE = 64 << 20, REPS = 8 };
    size_t after_first = 0;
    /* This measures libc, so the harness heap steps aside for it. */
    const Allocator *harness = mem_set_heap(NULL);
    for (size_t r = 0; r < REPS; r++) {
        char *p = NULL;
        size_t cap = 0;
        while (cap < SIZE) {
            size_t next = cap != 0 ? cap * 2 : 16;
            char *q = mem_realloc(NULL, p, cap, next);
            if (q == NULL) mem_set_heap(harness);
            REQUIRE(q != NULL);
            memset(q + cap, 1, next - cap);
            p = q;
            cap = next;
        }
        (void)mem_realloc(NULL, p, cap, 0);
        if (r == 0) after_first = footprint();
    }
    /* libmalloc may cache a freed block, but later passes must reuse it. */
    CHECK(footprint() < after_first + SIZE);
    mem_set_heap(harness);
}

/* ASan aborts on a request it cannot serve, so this needs the same gate. */
TEST(libc_oom_leaves_block) {
    const Allocator *harness = mem_set_heap(NULL);
    char *p = mem_realloc(NULL, NULL, 0, 8);
    CHECK(p != NULL);
    if (p != NULL) memcpy(p, "intact", 7);
    /* Unused, the malloc may be removed and its null check folded to false,
     * as clang -O2 does. Storing it through volatile keeps it real. */
    void *volatile grown = mem_realloc(NULL, p, 8, SIZE_MAX);
    CHECK(grown == NULL);
    CHECK_EQ_STR(p, "intact");
    p = mem_realloc(NULL, p, 8, 0);
    mem_set_heap(harness);
}
#endif

TEST(arena_alloc_and_free) {
    /* [arena] */
    Arena arena = { 0 };
    int *squares = arena_alloc(&arena, 100 * sizeof *squares, alignof(int));
    REQUIRE(squares != NULL);
    for (int i = 0; i < 100; i++) squares[i] = i * i;
    CHECK_EQ_INT(squares[99], 9801);
    arena_free(&arena); /* one call releases every allocation */
    /* [arena] */
}

TEST(arena_alignment) {
    Arena a = { 0 };
    for (size_t align = 1; align <= 4096; align *= 2) {
        REQUIRE(arena_alloc(&a, 1, 1) != NULL); /* knock the mark off */
        void *p = arena_alloc(&a, 3, align);
        REQUIRE(p != NULL);
        CHECK_EQ_INT((uintptr_t)p % align, 0);
    }
    arena_free(&a);
}

TEST(arena_large_request_gets_own_block) {
    Arena a = { 0 };
    char *small = arena_alloc(&a, 16, 1);
    REQUIRE(small != NULL);
    memcpy(small, "still here", 11);

    size_t n = 3 * MEM_ARENA_BLOCK;
    char *big = arena_alloc(&a, n, 1);
    REQUIRE(big != NULL);
    memset(big, 'x', n);
    CHECK(a.head->cap >= n);
    CHECK(a.head->prev != NULL);
    CHECK_EQ_STR(small, "still here");

    CHECK(arena_alloc(&a, 16, 1) != NULL);
    arena_free(&a);
}

TEST(arena_allocator_grows_last_in_place) {
    /* [allocator] */
    Arena arena = { 0 };
    const Allocator *al = arena_allocator(&arena);
    char *p = mem_realloc(al, NULL, 0, 16);
    REQUIRE(p != NULL);
    memcpy(p, "grow me", 8);
    char *q = mem_realloc(al, p, 16, 1000); /* newest, so it grows in place */
    CHECK(q == p);
    arena_free(&arena);
    /* [allocator] */
}

TEST(arena_allocator_copies_older) {
    Arena a = { 0 };
    const Allocator *al = arena_allocator(&a);
    char *p = mem_realloc(al, NULL, 0, 8);
    REQUIRE(p != NULL);
    memcpy(p, "older", 6);
    REQUIRE(mem_realloc(al, NULL, 0, 8) != NULL);

    char *q = mem_realloc(al, p, 8, 64);
    REQUIRE(q != NULL);
    CHECK(q != p);
    CHECK_EQ_STR(q, "older");

    /* Shrinking an older allocation also copies, and keeps the prefix. */
    char *r = mem_realloc(al, q, 64, 3);
    REQUIRE(r != NULL);
    CHECK_EQ_MEM(r, 3, "old", 3);
    arena_free(&a);
}

TEST(arena_allocator_free_last_rolls_back) {
    Arena a = { 0 };
    const Allocator *al = arena_allocator(&a);
    char *keep = mem_realloc(al, NULL, 0, 32);
    char *p = mem_realloc(al, NULL, 0, 100);
    REQUIRE(keep != NULL && p != NULL);
    size_t used = a.head->used;

    CHECK(mem_realloc(al, p, 100, 0) == NULL);
    CHECK(a.head->used < used);
    CHECK(mem_realloc(al, NULL, 0, 100) == p);

    /* Freeing anything but the newest allocation leaves the arena alone. */
    used = a.head->used;
    CHECK(mem_realloc(al, keep, 32, 0) == NULL);
    CHECK_EQ_INT(a.head->used, used);
    arena_free(&a);
}

TEST(arena_reset_keeps_one_block) {
    Arena a = { 0 };
    for (int i = 0; i < 4; i++) {
        REQUIRE(arena_alloc(&a, MEM_ARENA_BLOCK, 1) != NULL);
    }
    REQUIRE(a.head->prev != NULL);
    ArenaBlock *kept = a.head;

    arena_reset(&a);
    CHECK(a.head == kept);
    CHECK(a.head->prev == NULL);
    CHECK_EQ_INT(a.head->used, 0);
    CHECK(a.last == NULL);
    CHECK(arena_alloc(&a, 8, 1) == kept->data);
    arena_free(&a);
}

TEST(arena_free_twice_and_reuse) {
    Arena a = { 0 };
    const Allocator *al = arena_allocator(&a);
    REQUIRE(arena_alloc(&a, 64, 8) != NULL);
    arena_free(&a);
    arena_free(&a);
    CHECK(a.head == NULL);

    /* The allocator survives arena_free, so it still works. */
    CHECK(a.allocator.fn == al->fn && a.allocator.ctx == al->ctx);
    CHECK(mem_realloc(al, NULL, 0, 16) != NULL);
    arena_free(&a);
    arena_reset(&a); /* an empty arena accepts reset too */
}

TEST(arena_rejects_overflow) {
    Arena a = { 0 };
    CHECK(arena_alloc(&a, SIZE_MAX, 1) == NULL);
    CHECK(arena_alloc(&a, SIZE_MAX - 10, 64) == NULL);
    CHECK(a.head == NULL);

    const Allocator *al = arena_allocator(&a);
    char *p = mem_realloc(al, NULL, 0, 8);
    REQUIRE(p != NULL);
    memcpy(p, "intact", 7);
    CHECK(mem_realloc(al, p, 8, SIZE_MAX) == NULL);
    CHECK_EQ_STR(p, "intact");
    arena_free(&a);
}

TEST(arena_oom_leaves_state) {
    Arena a = { 0 };
    test_fail_alloc_after(0);
    CHECK(arena_alloc(&a, 8, 1) == NULL);
    test_fail_alloc_after(-1);
    CHECK(a.head == NULL && a.last == NULL);

    char *p = arena_alloc(&a, 8, 1);
    REQUIRE(p != NULL);
    memcpy(p, "kept", 5);
    test_fail_alloc_after(0);
    CHECK(arena_alloc(&a, 2 * MEM_ARENA_BLOCK, 1) == NULL);
    test_fail_alloc_after(-1);
    CHECK(a.last == p);
    CHECK_EQ_STR(p, "kept");
    arena_free(&a);
}

/* --- Arena marks --------------------------------------------------------- */

static size_t chain_len(const Arena *a) {
    size_t n = 0;
    for (const ArenaBlock *b = a->head; b != NULL; b = b->prev) n++;
    return n;
}

TEST(mark_on_empty_arena) {
    Arena a = { 0 };
    const Allocator *al = arena_allocator(&a);
    ArenaMark m = arena_mark(&a);
    REQUIRE(arena_alloc(&a, 8, 1) != NULL);
    REQUIRE(arena_alloc(&a, 2 * MEM_ARENA_BLOCK, 1) != NULL);
    arena_restore(&a, m);
    CHECK(a.head == NULL);
    CHECK(a.last == NULL);
    CHECK(a.allocator.fn == al->fn && a.allocator.ctx == al->ctx);

    CHECK(arena_alloc(&a, 8, 1) != NULL);
    arena_free(&a);
}

TEST(restore_rewinds_within_block) {
    Arena a = { 0 };
    REQUIRE(arena_alloc(&a, 24, 8) != NULL);
    ArenaBlock *head = a.head;
    ArenaMark m = arena_mark(&a);
    void *p = arena_alloc(&a, 40, 8);
    REQUIRE(p != NULL);
    arena_restore(&a, m);
    CHECK(a.head == head);
    CHECK(arena_alloc(&a, 40, 8) == p);
    arena_free(&a);
}

TEST(restore_frees_newer_blocks) {
    Arena a = { 0 };
    REQUIRE(arena_alloc(&a, 16, 1) != NULL);
    ArenaBlock *head = a.head;
    size_t used = head->used, len = chain_len(&a);
    ArenaMark m = arena_mark(&a);
    for (int i = 0; i < 3; i++) {
        REQUIRE(arena_alloc(&a, 2 * MEM_ARENA_BLOCK, 1) != NULL);
    }
    CHECK_EQ_INT(chain_len(&a), len + 3);
    arena_restore(&a, m);
    CHECK(a.head == head);
    CHECK_EQ_INT(chain_len(&a), len);
    CHECK_EQ_INT(a.head->used, used);
    arena_free(&a);
}

TEST(restore_keeps_older_data) {
    Arena a = { 0 };
    char *s = arena_alloc(&a, 16, 1);
    REQUIRE(s != NULL);
    memcpy(s, "before the mark", 16);
    ArenaMark m = arena_mark(&a);
    REQUIRE(arena_alloc(&a, 100, 1) != NULL);
    REQUIRE(arena_alloc(&a, 2 * MEM_ARENA_BLOCK, 1) != NULL);
    arena_restore(&a, m);
    CHECK_EQ_STR(s, "before the mark");
    arena_free(&a);
}

TEST(mark_stops_in_place_growth) {
    Arena a = { 0 };
    const Allocator *al = arena_allocator(&a);
    char *p = mem_realloc(al, NULL, 0, 16);
    REQUIRE(p != NULL);
    CHECK(mem_realloc(al, p, 16, 32) == p); /* no mark: grows in place */

    memcpy(p, "grow past mark", 15);
    ArenaMark m = arena_mark(&a);
    char *q = mem_realloc(al, p, 32, 64);
    REQUIRE(q != NULL);
    CHECK(q != p);
    CHECK_EQ_STR(q, "grow past mark");
    arena_restore(&a, m);
    arena_free(&a);
}

TEST(marks_nest) {
    Arena a = { 0 };
    REQUIRE(arena_alloc(&a, 16, 1) != NULL);
    ArenaBlock *head_a = a.head;
    size_t used_a = head_a->used, len_a = chain_len(&a);
    ArenaMark outer = arena_mark(&a);

    REQUIRE(arena_alloc(&a, 32, 1) != NULL);
    REQUIRE(arena_alloc(&a, 2 * MEM_ARENA_BLOCK, 1) != NULL);
    ArenaBlock *head_b = a.head;
    size_t used_b = head_b->used, len_b = chain_len(&a);
    ArenaMark inner = arena_mark(&a);

    REQUIRE(arena_alloc(&a, 8, 1) != NULL);
    REQUIRE(arena_alloc(&a, 2 * MEM_ARENA_BLOCK, 1) != NULL);
    arena_restore(&a, inner);
    CHECK(a.head == head_b);
    CHECK_EQ_INT(a.head->used, used_b);
    CHECK_EQ_INT(chain_len(&a), len_b);

    arena_restore(&a, outer);
    CHECK(a.head == head_a);
    CHECK_EQ_INT(a.head->used, used_a);
    CHECK_EQ_INT(chain_len(&a), len_a);
    arena_free(&a);
}

TEST(mark_after_reset_restores) {
    Arena a = { 0 };
    for (int i = 0; i < 3; i++) {
        REQUIRE(arena_alloc(&a, MEM_ARENA_BLOCK, 1) != NULL);
    }
    arena_reset(&a);
    ArenaMark m = arena_mark(&a);
    ArenaBlock *head = a.head;
    void *p = arena_alloc(&a, 64, 8);
    REQUIRE(p != NULL);
    REQUIRE(arena_alloc(&a, 2 * MEM_ARENA_BLOCK, 1) != NULL);
    arena_restore(&a, m);
    CHECK(a.head == head);
    CHECK_EQ_INT(chain_len(&a), 1);
    CHECK(arena_alloc(&a, 64, 8) == p);
    arena_free(&a);
}

TEST(restore_drops_scoped_str) {
    Arena a = { 0 };
    REQUIRE(arena_alloc(&a, 8, 1) != NULL);
    ArenaMark m = arena_mark(&a);
    Str s = { .alloc = arena_allocator(&a) };
    for (int i = 0; i < 50; i++) REQUIRE(str_append(&s, SV("scoped text ")));
    void *first = s.data; /* the newest block grows in place */
    arena_restore(&a, m);
    CHECK(arena_alloc(&a, 8, alignof(max_align_t)) == first);
    arena_free(&a);
}

/* --- Heap hook ----------------------------------------------------------- */

typedef struct HeapCount {
    int allocs, frees;
} HeapCount;

static void *counting_heap_fn(void *ctx, void *old, size_t old_size,
                              size_t new_size) {
    HeapCount *c = ctx;
    (void)old_size;
    if (new_size == 0) {
        c->frees++;
        free(old);
        return NULL;
    }
    c->allocs++;
    return realloc(old, new_size);
}

TEST(heap_hook_routes_null_allocator) {
    HeapCount c = { 0 };
    Allocator counting = { .fn = counting_heap_fn, .ctx = &c };
    const Allocator *prev = mem_set_heap(&counting);

    /* No REQUIRE: returning early would leave `c` installed after it dies. */
    char *p = mem_realloc(NULL, NULL, 0, 8);
    CHECK(p != NULL);
    CHECK_EQ_INT(c.allocs, 1);
    CHECK(mem_realloc(NULL, p, 8, 0) == NULL);
    CHECK_EQ_INT(c.frees, 1);

    Arena a = { 0 };
    CHECK(arena_alloc(&a, 8, 1) != NULL);
    CHECK_EQ_INT(c.allocs, 2);
    arena_free(&a);
    CHECK_EQ_INT(c.frees, 2);

    CHECK(mem_set_heap(NULL) == &counting);
    p = mem_realloc(NULL, NULL, 0, 8);
    CHECK(p != NULL);
    CHECK(mem_realloc(NULL, p, 8, 0) == NULL);
    CHECK_EQ_INT(c.allocs, 2);
    CHECK_EQ_INT(c.frees, 2);
    CHECK(mem_set_heap(prev) == NULL);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(heap_realloc_and_free);
    RUN(libc_realloc_and_free);
#if MEM_FOOTPRINT_TESTS
    RUN(heap_growth_returns_memory);
    RUN(libc_oom_leaves_block);
#endif
    RUN(arena_alloc_and_free);
    RUN(arena_alignment);
    RUN(arena_large_request_gets_own_block);
    RUN(arena_allocator_grows_last_in_place);
    RUN(arena_allocator_copies_older);
    RUN(arena_allocator_free_last_rolls_back);
    RUN(arena_reset_keeps_one_block);
    RUN(arena_free_twice_and_reuse);
    RUN(arena_rejects_overflow);
    RUN(arena_oom_leaves_state);
    RUN(mark_on_empty_arena);
    RUN(restore_rewinds_within_block);
    RUN(restore_frees_newer_blocks);
    RUN(restore_keeps_older_data);
    RUN(mark_stops_in_place_growth);
    RUN(marks_nest);
    RUN(mark_after_reset_restores);
    RUN(restore_drops_scoped_str);
    RUN(heap_hook_routes_null_allocator);
    return TEST_REPORT();
}
