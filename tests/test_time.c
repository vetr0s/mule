/* Tests for src/time/time.c.
 *
 * Bracketed names in comment pairs mark regions that src/time/time.h pulls
 * into the Doxygen reference with @snippet. See tests/test_str.c for the rules.
 */
#include "test/test.h"

#include "log/log.c"
#include "mem/mem.c"
#include "str/str.c"
#include "time/time.c"

#include <time.h>

TEST(measures_a_sleep) {
    /* [time] */
    uint64_t start = time_now_ns();
    nanosleep(&(struct timespec){ .tv_nsec = 2000000 }, NULL); /* 2 ms */
    uint64_t elapsed = time_now_ns() - start;
    /* [time] */
    CHECK(elapsed >= 2000000);
    CHECK(elapsed < 2000000000); /* well under 2 s, even on a loaded machine */
}

TEST(never_goes_backwards) {
    uint64_t prev = time_now_ns();
    for (int i = 0; i < 100000; i++) {
        uint64_t now = time_now_ns();
        REQUIRE(now >= prev);
        prev = now;
    }
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(measures_a_sleep);
    RUN(never_goes_backwards);
    return TEST_REPORT();
}
