#pragma once
/**
 * @file
 * @brief Rand: a small, fast, seeded pseudo-random generator.
 *
 *   - splitmix64, so the same seed gives the same sequence on every platform,
 *     unlike rand(). Use it for tests, simulations and shuffles.
 *   - Not for secrets. Use the operating system's generator for keys and
 *     tokens.
 *   - A zero-initialised Rand is valid. Set `state` to pick a seed.
 *
 * @snippet{trimleft} test_rand.c rand
 */

#include "base/base.h"

/** Generator state. Copy it to fork a sequence. */
typedef struct Rand {
    uint64_t state; /**< The seed, advanced by every call. */
} Rand;

/** The next 64 random bits. */
[[nodiscard]] uint64_t rand_next(Rand *r);
/** A uniform value in [0, @p n). @p n must not be 0. */
[[nodiscard]] uint64_t rand_below(Rand *r, uint64_t n);
