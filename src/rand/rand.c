#include "rand/rand.h"

#include <assert.h>

uint64_t rand_next(Rand *r) {
    uint64_t z = (r->state += 0x9E3779B97F4A7C15u);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
    return z ^ (z >> 31);
}

uint64_t rand_below(Rand *r, uint64_t n) {
    assert(n != 0);
    /* Reject the low values that would make some results more likely. */
    uint64_t threshold = -n % n;
    for (;;) {
        uint64_t x = rand_next(r);
        if (x >= threshold) return x % n;
    }
}
