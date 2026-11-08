/* Tests for src/rand/rand.c.
 *
 * Bracketed names in comment pairs mark regions that src/rand/rand.h pulls
 * into the Doxygen reference with @snippet. See tests/test_str.c for the rules.
 */
#include "test/test.h"

#include "log/log.c"
#include "mem/mem.c"
#include "rand/rand.c"
#include "str/str.c"

TEST(matches_reference_splitmix64) {
    Rand r = { 0 };
    CHECK(rand_next(&r) == 0xe220a8397b1dcdafu);
    CHECK(rand_next(&r) == 0x6e789e6aa1b965f4u);
    CHECK(rand_next(&r) == 0x06c45d188009454fu);
}

TEST(same_seed_same_sequence) {
    /* [rand] */
    Rand dice = { .state = 2026 };
    uint64_t roll = 1 + rand_below(&dice, 6); /* 1 to 6 */
    /* [rand] */
    CHECK(roll >= 1 && roll <= 6);

    Rand a = { 42 }, b = { 42 };
    for (int i = 0; i < 100; i++) CHECK(rand_next(&a) == rand_next(&b));
}

TEST(below_stays_in_range_and_spreads) {
    Rand r = { 9 };
    int counts[10] = { 0 };
    for (int i = 0; i < 100000; i++) {
        uint64_t v = rand_below(&r, 10);
        REQUIRE(v < 10);
        counts[v]++;
    }
    /* Each bucket expects 10000. Five standard deviations is about 475. */
    for (int i = 0; i < 10; i++) CHECK(counts[i] > 9500 && counts[i] < 10500);

    CHECK_EQ_INT(rand_below(&r, 1), 0);
    uint64_t huge = (UINT64_C(1) << 63) + 1; /* rejects almost half the draws */
    for (int i = 0; i < 1000; i++) CHECK(rand_below(&r, huge) < huge);
}

/* With n near two thirds of 2^64, plain `x % n` lands in the lower half of
 * the range two times in three. Rejection brings it back to one in two. */
TEST(below_is_unbiased) {
    Rand r = { 5 };
    uint64_t n = 0xAAAAAAAAAAAAAAAAu;
    int low = 0;
    for (int i = 0; i < 10000; i++) low += rand_below(&r, n) < n / 2;
    CHECK(low > 4700 && low < 5300);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(matches_reference_splitmix64);
    RUN(same_seed_same_sequence);
    RUN(below_stays_in_range_and_spreads);
    RUN(below_is_unbiased);
    return TEST_REPORT();
}
