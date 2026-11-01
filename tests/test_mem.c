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
    char *p = mem_realloc(nullptr, nullptr, 0, 8);
    REQUIRE(p != nullptr);
    memcpy(p, "abcdefg", 8);
    p = mem_realloc(nullptr, p, 8, 4096);
    REQUIRE(p != nullptr);
    CHECK_EQ_STR(p, "abcdefg");
    CHECK(mem_realloc(nullptr, p, 4096, 0) == nullptr);
}

/* --- Arena --------------------------------------------------------------- */

#if MEM_FOOTPRINT_TESTS
/* Bytes of memory the kernel charges to this process right now. */
static size_t footprint(void) {
    task_vm_info_data_t info;
    mach_msg_type_number_t n = TASK_VM_INFO_COUNT;
    kern_return_t err = task_info(mach_task_self(), TASK_VM_INFO,
                                  (task_info_t)&info, &n);
    assert(err == KERN_SUCCESS);
    return (size_t)info.phys_footprint;
}

/* macOS 26 libmalloc kept every large block grown by realloc dirty after
 * free, so a buffer grown and freed in a loop leaked its size each pass. */
TEST(heap_growth_returns_memory) {
    enum : size_t { SIZE = 64 << 20, REPS = 8 };
    size_t after_first = 0;
    for (size_t r = 0; r < REPS; r++) {
        char *p = nullptr;
        size_t cap = 0;
        while (cap < SIZE) {
            size_t next = cap != 0 ? cap * 2 : 16;
            char *q = mem_realloc(nullptr, p, cap, next);
            REQUIRE(q != nullptr);
            memset(q + cap, 1, next - cap);
            p = q;
            cap = next;
        }
        (void)mem_realloc(nullptr, p, cap, 0);
        if (r == 0) after_first = footprint();
    }
    /* libmalloc may cache a freed block, but later passes must reuse it. */
    CHECK(footprint() < after_first + SIZE);
}
#endif

TEST(arena_alloc_and_free) {
    /* [arena] */
    Arena arena = {};
    int *squares = arena_alloc(&arena, 100 * sizeof *squares, alignof(int));
    REQUIRE(squares != nullptr);
    for (int i = 0; i < 100; i++) squares[i] = i * i;
    CHECK_EQ_INT(squares[99], 9801);
    arena_free(&arena); /* one call releases every allocation */
    /* [arena] */
}

TEST(arena_alignment) {
    Arena a = {};
    for (size_t align = 1; align <= 4096; align *= 2) {
        REQUIRE(arena_alloc(&a, 1, 1) != nullptr); /* knock the mark off */
        void *p = arena_alloc(&a, 3, align);
        REQUIRE(p != nullptr);
        CHECK_EQ_INT((uintptr_t)p % align, 0);
    }
    arena_free(&a);
}

TEST(arena_large_request_gets_own_block) {
    Arena a = {};
    char *small = arena_alloc(&a, 16, 1);
    REQUIRE(small != nullptr);
    memcpy(small, "still here", 11);

    size_t n = 3 * MEM_ARENA_BLOCK;
    char *big = arena_alloc(&a, n, 1);
    REQUIRE(big != nullptr);
    memset(big, 'x', n);
    CHECK(a.head->cap >= n);
    CHECK(a.head->prev != nullptr);
    CHECK_EQ_STR(small, "still here");

    CHECK(arena_alloc(&a, 16, 1) != nullptr);
    arena_free(&a);
}

TEST(arena_allocator_grows_last_in_place) {
    /* [allocator] */
    Arena arena = {};
    const Allocator *al = arena_allocator(&arena);
    char *p = mem_realloc(al, nullptr, 0, 16);
    REQUIRE(p != nullptr);
    memcpy(p, "grow me", 8);
    char *q = mem_realloc(al, p, 16, 1000); /* newest, so it grows in place */
    CHECK(q == p);
    arena_free(&arena);
    /* [allocator] */
}

TEST(arena_allocator_copies_older) {
    Arena a = {};
    const Allocator *al = arena_allocator(&a);
    char *p = mem_realloc(al, nullptr, 0, 8);
    REQUIRE(p != nullptr);
    memcpy(p, "older", 6);
    REQUIRE(mem_realloc(al, nullptr, 0, 8) != nullptr);

    char *q = mem_realloc(al, p, 8, 64);
    REQUIRE(q != nullptr);
    CHECK(q != p);
    CHECK_EQ_STR(q, "older");

    /* Shrinking an older allocation also copies, and keeps the prefix. */
    char *r = mem_realloc(al, q, 64, 3);
    REQUIRE(r != nullptr);
    CHECK_EQ_MEM(r, 3, "old", 3);
    arena_free(&a);
}

TEST(arena_allocator_free_last_rolls_back) {
    Arena a = {};
    const Allocator *al = arena_allocator(&a);
    char *keep = mem_realloc(al, nullptr, 0, 32);
    char *p = mem_realloc(al, nullptr, 0, 100);
    REQUIRE(keep != nullptr && p != nullptr);
    size_t used = a.head->used;

    CHECK(mem_realloc(al, p, 100, 0) == nullptr);
    CHECK(a.head->used < used);
    CHECK(mem_realloc(al, nullptr, 0, 100) == p);

    /* Freeing anything but the newest allocation leaves the arena alone. */
    used = a.head->used;
    CHECK(mem_realloc(al, keep, 32, 0) == nullptr);
    CHECK_EQ_INT(a.head->used, used);
    arena_free(&a);
}

TEST(arena_reset_keeps_one_block) {
    Arena a = {};
    for (int i = 0; i < 4; i++) {
        REQUIRE(arena_alloc(&a, MEM_ARENA_BLOCK, 1) != nullptr);
    }
    REQUIRE(a.head->prev != nullptr);
    ArenaBlock *kept = a.head;

    arena_reset(&a);
    CHECK(a.head == kept);
    CHECK(a.head->prev == nullptr);
    CHECK_EQ_INT(a.head->used, 0);
    CHECK(a.last == nullptr);
    CHECK(arena_alloc(&a, 8, 1) == kept->data);
    arena_free(&a);
}

TEST(arena_free_twice_and_reuse) {
    Arena a = {};
    const Allocator *al = arena_allocator(&a);
    REQUIRE(arena_alloc(&a, 64, 8) != nullptr);
    arena_free(&a);
    arena_free(&a);
    CHECK(a.head == nullptr);

    /* The allocator survives arena_free, so it still works. */
    CHECK(a.allocator.fn == al->fn && a.allocator.ctx == al->ctx);
    CHECK(mem_realloc(al, nullptr, 0, 16) != nullptr);
    arena_free(&a);
    arena_reset(&a); /* an empty arena accepts reset too */
}

TEST(arena_rejects_overflow) {
    Arena a = {};
    CHECK(arena_alloc(&a, SIZE_MAX, 1) == nullptr);
    CHECK(arena_alloc(&a, SIZE_MAX - 10, 64) == nullptr);
    CHECK(a.head == nullptr);

    const Allocator *al = arena_allocator(&a);
    char *p = mem_realloc(al, nullptr, 0, 8);
    REQUIRE(p != nullptr);
    memcpy(p, "intact", 7);
    CHECK(mem_realloc(al, p, 8, SIZE_MAX) == nullptr);
    CHECK_EQ_STR(p, "intact");
    arena_free(&a);
}

TEST(arena_oom_leaves_state) {
    Arena a = {};
    test_fail_alloc_after(0);
    CHECK(arena_alloc(&a, 8, 1) == nullptr);
    test_fail_alloc_after(-1);
    CHECK(a.head == nullptr && a.last == nullptr);

    char *p = arena_alloc(&a, 8, 1);
    REQUIRE(p != nullptr);
    memcpy(p, "kept", 5);
    test_fail_alloc_after(0);
    CHECK(arena_alloc(&a, 2 * MEM_ARENA_BLOCK, 1) == nullptr);
    test_fail_alloc_after(-1);
    CHECK(a.last == p);
    CHECK_EQ_STR(p, "kept");
    arena_free(&a);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(heap_realloc_and_free);
#if MEM_FOOTPRINT_TESTS
    RUN(heap_growth_returns_memory);
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
    return TEST_REPORT();
}
