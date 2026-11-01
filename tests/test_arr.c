/* Tests for src/arr/arr.c.
 *
 * Bracketed names in comment pairs mark regions that src/arr/arr.h pulls into
 * the Doxygen reference with @snippet. See tests/test_str.c for the rules.
 */
#include "test.h"

#include "arr/arr.c"
#include "log/log.c"
#include "mem/mem.c"
#include "str/str.c"

typedef Arr(int) IntArr;

typedef struct Point {
    int x, y;
} Point;
typedef Arr(Point) PointArr;

/* True when `a` holds exactly the `n` ints in `want`. */
static bool holds(const IntArr *a, const int *want, size_t n) {
    return a->len == n &&
           (n == 0 || memcmp(a->data, want, n * sizeof *want) == 0);
}

TEST(push_pop_and_free) {
    /* [arr] */
    IntArr squares = {};
    for (int i = 0; i < 100; i++) CHECK(arr_push(&squares, i * i));
    CHECK_EQ_INT(squares.len, 100);
    CHECK_EQ_INT(squares.data[99], 9801);
    CHECK_EQ_INT(arr_pop(&squares), 9801);
    arr_free(&squares);
    /* [arr] */
    CHECK(squares.data == nullptr && squares.len == 0 && squares.capacity == 0);
    arr_free(&squares);
}

TEST(structs) {
    PointArr pts = {};
    CHECK(arr_push(&pts, ((Point){ 1, 2 })));
    CHECK(arr_push(&pts, ((Point){ 3, 4 })));
    Point last = arr_pop(&pts);
    CHECK_EQ_INT(last.x, 3);
    CHECK_EQ_INT(last.y, 4);
    CHECK_EQ_INT(pts.data[0].y, 2);
    arr_free(&pts);
}

TEST(push_reads_own_element_across_growth) {
    IntArr a = {};
    REQUIRE(arr_push(&a, 7));
    while (a.len < a.capacity) REQUIRE(arr_push(&a, 0));
    int *before = a.data;
    REQUIRE(arr_push(&a, a.data[0])); /* this push grows the array */
    CHECK(a.data != before);
    CHECK_EQ_INT(a.data[a.len - 1], 7);
    arr_free(&a);
}

TEST(insert_and_remove) {
    IntArr a = {};
    REQUIRE(arr_insert(&a, 0, 2)); /* into an empty array */
    REQUIRE(arr_insert(&a, 0, 1)); /* at the front */
    REQUIRE(arr_insert(&a, 2, 4)); /* at the end */
    REQUIRE(arr_insert(&a, 2, 3)); /* in the middle */
    CHECK(holds(&a, (int[]){ 1, 2, 3, 4 }, 4));

    arr_remove(&a, 0);
    CHECK(holds(&a, (int[]){ 2, 3, 4 }, 3));
    arr_remove(&a, 1);
    CHECK(holds(&a, (int[]){ 2, 4 }, 2));
    arr_remove(&a, 1);
    CHECK(holds(&a, (int[]){ 2 }, 1));
    arr_remove(&a, 0);
    CHECK(holds(&a, nullptr, 0));
    arr_free(&a);
}

TEST(reserve_and_clear) {
    IntArr a = {};
    REQUIRE(arr_reserve(&a, 1000));
    CHECK(a.capacity >= 1000);
    int *data = a.data;
    for (int i = 0; i < 1000; i++) REQUIRE(arr_push(&a, i));
    CHECK(a.data == data); /* no reallocation after reserving */

    arr_clear(&a);
    CHECK_EQ_INT(a.len, 0);
    CHECK(a.data == data && a.capacity >= 1000);
    arr_free(&a);
}

TEST(reserve_rejects_overflow) {
    IntArr a = {};
    REQUIRE(arr_push(&a, 5));
    IntArr before = a;
    CHECK(!arr_reserve(&a, SIZE_MAX));     /* len + n overflows */
    CHECK(!arr_reserve(&a, SIZE_MAX / 2)); /* n * sizeof(int) overflows */
    CHECK(a.data == before.data && a.len == 1 && a.capacity == before.capacity);
    CHECK_EQ_INT(a.data[0], 5);
    arr_free(&a);
}

TEST(oom_leaves_array_unchanged) {
    IntArr a = {};
    while (a.len < 8) REQUIRE(arr_push(&a, (int)a.len));
    REQUIRE(a.len == a.capacity); /* the next push must grow */
    IntArr before = a;

    test_fail_alloc_after(0);
    CHECK(!arr_push(&a, 99));
    CHECK(!arr_insert(&a, 0, 99));
    test_fail_alloc_after(-1);
    CHECK(a.data == before.data &&
          a.len == before.len &&
          a.capacity == before.capacity);
    CHECK(holds(&a, (int[]){ 0, 1, 2, 3, 4, 5, 6, 7 }, 8));
    arr_free(&a);
}

TEST(arena_backed) {
    Arena arena = {};
    IntArr a = { .alloc = arena_allocator(&arena) };
    for (int i = 0; i < 10000; i++) REQUIRE(arr_push(&a, i));
    /* Every doubling extended the same allocation in place. */
    CHECK(arena.head->prev == nullptr);
    CHECK_EQ_INT(arena.head->used, a.capacity * sizeof(int));
    CHECK_EQ_INT(a.data[9999], 9999);

    const Allocator *al = a.alloc;
    arr_free(&a);
    CHECK(a.alloc == al);
    arena_free(&arena);
}

/* splitmix64, matching tests/test_str.c. */
static uint64_t next_random(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15u);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
    return z ^ (z >> 31);
}

/* Random operations, checked after every step against a plain array. */
static void check_against_model(const Allocator *alloc) {
    int model[600];
    size_t n = 0;
    IntArr a = { .alloc = alloc };
    uint64_t rng = 3;

    for (int step = 0; step < 20000; step++) {
        uint64_t r = next_random(&rng);
        int v = (int)(r >> 32);
        bool ok = true;
        switch (r % 6) {
        case 0:
        case 1:
            if (n == 500) break;
            ok = arr_push(&a, v);
            model[n++] = v;
            break;
        case 2:
            if (n == 0) break;
            ok = arr_pop(&a) == model[--n];
            break;
        case 3: {
            if (n == 500) break;
            size_t i = (size_t)(r >> 8) % (n + 1);
            ok = arr_insert(&a, i, v);
            memmove(model + i + 1, model + i, (n - i) * sizeof *model);
            model[i] = v;
            n++;
            break;
        }
        case 4: {
            if (n == 0) break;
            size_t i = (size_t)(r >> 8) % n;
            arr_remove(&a, i);
            memmove(model + i, model + i + 1, (n - i - 1) * sizeof *model);
            n--;
            break;
        }
        default:
            if (r % 97 == 0) {
                arr_clear(&a);
                n = 0;
            }
            break;
        }
        bool same = ok && holds(&a, model, n) && a.capacity >= a.len;
        if (!same) printf("  diverged at step %d\n", step);
        REQUIRE(same);
    }
    arr_free(&a);
}

TEST(matches_model_under_random_ops) {
    check_against_model(nullptr);
}

TEST(matches_model_in_arena) {
    Arena arena = {};
    check_against_model(arena_allocator(&arena));
    arena_free(&arena);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(push_pop_and_free);
    RUN(structs);
    RUN(push_reads_own_element_across_growth);
    RUN(insert_and_remove);
    RUN(reserve_and_clear);
    RUN(reserve_rejects_overflow);
    RUN(oom_leaves_array_unchanged);
    RUN(arena_backed);
    RUN(matches_model_under_random_ops);
    RUN(matches_model_in_arena);
    return TEST_REPORT();
}
