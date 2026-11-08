/* Tests for src/map/map.c.
 *
 * Bracketed names in comment pairs mark regions that src/map/map.h pulls into
 * the Doxygen reference with @snippet. See tests/test_str.c for the rules.
 */
#include "test/test.h"

#include "arr/arr.c"
#include "log/log.c"
#include "map/map.c"
#include "mem/mem.c"
#include "rand/rand.c"
#include "str/str.c"

/* [map_type] */
typedef struct WordCount {
    StrView key; /* first, as every map entry must start */
    size_t count;
} WordCount;
typedef Map(WordCount) WordMap;
/* [map_type] */

typedef struct Word {
    StrView key;
} Word;
typedef Map(Word) WordSet;

TEST(count_words) {
    /* [map] */
    WordMap counts = { 0 };
    StrView rest = SV("the cat saw the other cat"), word;
    while (sv_token_next(&rest, SV(" "), &word)) {
        size_t i;
        REQUIRE(map_put(&counts, word, &i));
        counts.data[i].count++;
    }
    CHECK_EQ_INT(counts.len, 4); /* the, cat, saw, other, in that order */

    size_t i;
    CHECK(map_find(&counts, SV("cat"), &i));
    CHECK_EQ_INT(counts.data[i].count, 2);
    CHECK(!map_find(&counts, SV("dog"), &i));
    map_free(&counts);
    /* [map] */
}

TEST(entries_keep_insertion_order) {
    WordMap m = { 0 };
    const char *words[] = { "b", "a", "c", "a", "b", "d" };
    for (size_t w = 0; w < 6; w++) {
        size_t i;
        REQUIRE(map_put(&m, sv_from_cstr(words[w]), &i));
        m.data[i].count++;
    }
    REQUIRE(m.len == 4);
    const char *order[] = { "b", "a", "c", "d" };
    const size_t counts[] = { 2, 2, 1, 1 };
    for (size_t i = 0; i < 4; i++) {
        CHECK(str_eq(m.data[i].key, sv_from_cstr(order[i])));
        CHECK_EQ_INT(m.data[i].count, counts[i]);
    }
    map_free(&m);
}

TEST(own_keys_in_arena) {
    FILE *f = tmpfile();
    REQUIRE(f != NULL);
    fputs("b\na\nb\nc\na\n", f);
    rewind(f);

    /* [own_keys] */
    Arena keys = { 0 };
    WordSet seen = { 0 };
    Str line = { 0 };
    while (str_read_line(&line, f)) {
        size_t i;
        if (map_find(&seen, str_view(&line), &i)) continue;
        Str copy = { .alloc = arena_allocator(&keys) };
        REQUIRE(str_from(&copy, str_view(&line)));
        REQUIRE(map_put(&seen, str_view(&copy), &i));
    }
    CHECK_EQ_INT(seen.len, 3); /* b, a, c */
    str_free(&line);
    map_free(&seen);
    arena_free(&keys);
    /* [own_keys] */
    fclose(f);
}

TEST(empty_map) {
    WordMap m = { 0 };
    size_t i = 7;
    CHECK(!map_find(&m, SV("x"), &i));
    CHECK(!map_find(&m, SV(""), &i));
    CHECK_EQ_INT(i, 7);
    map_clear(&m);
    map_free(&m);
    map_free(&m);
    CHECK(m.data == NULL && m.slots == NULL && m.len == 0);
}

TEST(empty_key_is_a_key) {
    WordMap m = { 0 };
    size_t i;
    REQUIRE(map_put(&m, SV("a"), &i));
    CHECK(!map_find(&m, SV(""), &i));
    REQUIRE(map_put(&m, SV(""), &i));
    CHECK_EQ_INT(i, 1);
    REQUIRE(map_put(&m, (StrView){ 0 }, &i)); /* a null view is "" too */
    CHECK_EQ_INT(i, 1);
    CHECK_EQ_INT(m.len, 2);
    map_free(&m);
}

/* Keys "0" to "n-1", each in its own row, so views stay valid. */
enum { MANY = 20000 };
static char many_keys[MANY][8];

static StrView many_key(size_t k) {
    return sv_from_cstr(many_keys[k]);
}

TEST(many_keys_survive_growth) {
    WordMap m = { 0 };
    for (size_t k = 0; k < MANY; k++) {
        size_t i;
        REQUIRE(map_put(&m, many_key(k), &i));
        REQUIRE(i == k);
        m.data[i].count = k * 3;
    }
    CHECK(m.slot_count / 4 * 3 >= m.len);
    bool all_found = true;
    for (size_t k = 0; k < MANY; k++) {
        size_t i;
        all_found &= map_find(&m, many_key(k), &i) &&
                     i == k &&
                     m.data[i].count == k * 3;
    }
    CHECK(all_found);
    size_t i;
    CHECK(!map_find(&m, SV("20000"), &i));
    CHECK(!map_find(&m, SV("-1"), &i));
    map_free(&m);
}

TEST(reserve_then_put_does_not_allocate) {
    WordMap m = { 0 };
    REQUIRE(map_reserve(&m, 100));
    test_fail_alloc_after(0);
    for (size_t k = 0; k < 100; k++) {
        size_t i;
        REQUIRE(map_put(&m, many_key(k), &i));
    }
    test_fail_alloc_after(-1);
    map_clear(&m);
    CHECK_EQ_INT(m.len, 0);
    size_t i;
    CHECK(!map_find(&m, many_key(5), &i));
    CHECK(m.capacity >= 100);
    map_free(&m);
}

TEST(reserve_rejects_overflow) {
    WordMap m = { 0 };
    size_t i;
    REQUIRE(map_put(&m, SV("a"), &i));
    WordMap before = m;
    CHECK(!map_reserve(&m, SIZE_MAX));
    CHECK(!map_reserve(&m, MAP_MAX));
    CHECK(m.data == before.data &&
          m.slots == before.slots &&
          m.capacity == before.capacity &&
          m.slot_count == before.slot_count);
    map_free(&m);
}

/* Fail each allocation of a put in turn, from every size up to `MAX_LEN`,
 * and check that the map is byte for byte as it was. */
TEST(oom_leaves_map_unchanged) {
    enum { MAX_LEN = 40 };
    static WordCount data[MAX_LEN];
    static uint64_t slots[128];
    for (size_t len = 0; len < MAX_LEN; len++) {
        for (long k = 0;; k++) {
            WordMap m = { 0 };
            for (size_t n = 0; n < len; n++) {
                size_t i;
                REQUIRE(map_put(&m, many_key(n), &i));
                m.data[i].count = n;
            }
            REQUIRE(m.slot_count <= 128);
            WordMap before = m;
            if (m.len > 0) memcpy(data, m.data, m.len * sizeof *m.data);
            if (m.slot_count > 0)
                memcpy(slots, m.slots, m.slot_count * sizeof *m.slots);

            size_t i = 99;
            test_fail_alloc_after(k);
            bool ok = map_put(&m, many_key(len), &i);
            test_fail_alloc_after(-1);
            if (!ok) {
                bool same_entries = m.len == 0 ||
                                    memcmp(data, m.data,
                                           m.len * sizeof *m.data) == 0;
                bool same_slots = m.slot_count == 0 ||
                                  memcmp(slots, m.slots,
                                         m.slot_count * sizeof *m.slots) == 0;
                bool same = m.data == before.data &&
                            m.len == before.len &&
                            m.capacity == before.capacity &&
                            m.slots == before.slots &&
                            m.slot_count == before.slot_count &&
                            i == 99 &&
                            same_entries &&
                            same_slots;
                if (!same)
                    printf("  len %zu: changed after allocation %ld failed\n",
                           len, k + 1);
                CHECK(same);
            }
            map_free(&m);
            if (ok) break;
        }
    }
}

/* Random puts, finds, removes and clears over a small key pool, checked against
 * a plain array of the keys in the order they were added. */
static void check_against_model(const Allocator *alloc) {
    enum { POOL = 300 };
    size_t order[POOL], counts[POOL] = { 0 };
    size_t n = 0;
    WordMap m = { .alloc = alloc };
    Rand rng = { 5 };

    for (int step = 0; step < 30000; step++) {
        uint64_t r = rand_next(&rng);
        size_t k = (size_t)((r >> 16) % POOL);
        size_t i = SIZE_MAX, at = SIZE_MAX;
        for (size_t j = 0; j < n; j++)
            if (order[j] == k) at = j;

        bool ok = true;
        if (r % 500 == 0) {
            map_clear(&m);
            for (size_t j = 0; j < n; j++) counts[order[j]] = 0;
            n = 0;
        } else if (r % 2 == 0) {
            ok = map_put(&m, many_key(k), &i);
            if (ok && at == SIZE_MAX) {
                order[n] = k;
                at = n++;
            }
            ok = ok && i == at;
            if (ok) {
                m.data[i].count++;
                counts[k]++;
            }
        } else if (r % 4 == 1) {
            ok = map_remove(&m, many_key(k)) == (at != SIZE_MAX);
            if (ok && at != SIZE_MAX) {
                order[at] = order[--n];
                counts[k] = 0;
            }
        } else {
            bool found = map_find(&m, many_key(k), &i);
            ok = found == (at != SIZE_MAX) && (!found || i == at);
        }

        ok = ok && m.len == n;
        for (size_t j = 0; ok && j < n; j++)
            ok = str_eq(m.data[j].key, many_key(order[j])) &&
                 m.data[j].count == counts[order[j]];
        if (!ok) printf("  diverged at step %d\n", step);
        REQUIRE(ok);
    }
    map_free(&m);
}

TEST(matches_model_under_random_ops) {
    check_against_model(NULL);
}

TEST(matches_model_in_arena) {
    Arena arena = { 0 };
    check_against_model(arena_allocator(&arena));
    arena_free(&arena);
}

static WordMap filled(const Allocator *alloc, size_t n) {
    WordMap m = { .alloc = alloc };
    for (size_t k = 0; k < n; k++) {
        size_t i;
        bool ok = map_put(&m, many_key(k), &i);
        CHECK(ok);
        if (ok) m.data[i].count = k + 100;
    }
    return m;
}

static bool finds_with_value(WordMap *m, size_t k) {
    size_t i;
    return map_find(m, many_key(k), &i) && m->data[i].count == k + 100;
}

TEST(remove_deletes_key) {
    WordMap m = filled(NULL, 5);
    size_t i;
    CHECK(map_remove(&m, many_key(2)));
    CHECK(!map_find(&m, many_key(2), &i));
    CHECK_EQ_INT(m.len, 4);
    CHECK(finds_with_value(&m, 0) &&
          finds_with_value(&m, 1) &&
          finds_with_value(&m, 3) &&
          finds_with_value(&m, 4));
    map_free(&m);
}

TEST(remove_missing_key) {
    WordMap m = filled(NULL, 5);
    WordMap before = m;
    WordCount data[5];
    uint64_t slots[128];
    REQUIRE(m.slot_count <= 128);
    memcpy(data, m.data, sizeof data);
    memcpy(slots, m.slots, m.slot_count * sizeof *m.slots);
    CHECK(!map_remove(&m, many_key(50)));
    CHECK(m.len == 5 &&
          m.data == before.data &&
          m.slots == before.slots &&
          m.slot_count == before.slot_count);
    CHECK(memcmp(data, m.data, sizeof data) == 0);
    CHECK(memcmp(slots, m.slots, m.slot_count * sizeof *m.slots) == 0);
    map_free(&m);

    WordMap z = { 0 };
    CHECK(!map_remove(&z, many_key(1)));
    CHECK(z.len == 0 && z.data == NULL && z.slots == NULL);
}

TEST(remove_swaps_last_into_hole) {
    WordMap m = filled(NULL, 3);
    CHECK(map_remove(&m, many_key(0)));
    CHECK_EQ_INT(m.len, 2);
    CHECK(str_eq(m.data[0].key, many_key(2)) && m.data[0].count == 102);
    CHECK(str_eq(m.data[1].key, many_key(1)) && m.data[1].count == 101);
    size_t i;
    CHECK(map_find(&m, many_key(2), &i) && i == 0);

    CHECK(map_remove(&m, many_key(1))); /* the last entry */
    CHECK_EQ_INT(m.len, 1);
    CHECK(str_eq(m.data[0].key, many_key(2)) && m.data[0].count == 102);
    map_free(&m);
}

TEST(remove_with_colliding_keys) {
    enum { N = 5000 };
    WordMap m = filled(NULL, N);
    bool removed_all = true;
    for (size_t k = 100; k < 4000; k += 3)
        removed_all &= map_remove(&m, many_key(k));
    CHECK(removed_all);
    bool ok = true;
    size_t kept = 0;
    for (size_t k = 0; k < N; k++) {
        bool removed = k >= 100 && k < 4000 && (k - 100) % 3 == 0;
        size_t i;
        if (removed)
            ok &= !map_find(&m, many_key(k), &i);
        else
            ok &= finds_with_value(&m, k), kept++;
    }
    CHECK(ok);
    CHECK_EQ_INT(m.len, kept);
    map_free(&m);
}

TEST(remove_does_not_allocate) {
    WordMap m = filled(NULL, 50);
    test_fail_alloc_after(0);
    bool ok = map_remove(&m, many_key(7));
    test_fail_alloc_after(-1);
    CHECK(ok);
    map_free(&m);
}

TEST(remove_in_arena) {
    Arena arena = { 0 };
    WordMap m = filled(arena_allocator(&arena), 200);
    bool ok = true;
    for (size_t k = 0; k < 200; k += 2) ok &= map_remove(&m, many_key(k));
    CHECK(ok);
    CHECK_EQ_INT(m.len, 100);
    for (size_t k = 0; k < 200; k++) {
        size_t i;
        CHECK(map_find(&m, many_key(k), &i) == (k % 2 == 1));
    }
    arena_free(&arena);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    for (size_t k = 0; k < MANY; k++)
        snprintf(many_keys[k], sizeof many_keys[k], "%zu", k);
    RUN(count_words);
    RUN(entries_keep_insertion_order);
    RUN(own_keys_in_arena);
    RUN(empty_map);
    RUN(empty_key_is_a_key);
    RUN(many_keys_survive_growth);
    RUN(reserve_then_put_does_not_allocate);
    RUN(reserve_rejects_overflow);
    RUN(oom_leaves_map_unchanged);
    RUN(remove_deletes_key);
    RUN(remove_missing_key);
    RUN(remove_swaps_last_into_hole);
    RUN(remove_with_colliding_keys);
    RUN(remove_does_not_allocate);
    RUN(remove_in_arena);
    RUN(matches_model_under_random_ops);
    RUN(matches_model_in_arena);
    return TEST_REPORT();
}
