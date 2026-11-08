#pragma once
/**
 * @defgroup mod_rand Rand
 * @brief A small, fast, seeded pseudo-random generator.
 *
 * The generator is splitmix64, so the same seed gives the same sequence on
 * every platform, unlike rand(). Use it for tests, simulations and shuffles,
 * where a failure must be reproducible from its seed.
 *
 *   - A zero-initialised Rand is valid. Set `state` to pick a seed.
 *   - Each Rand is independent and holds no global state. Copy one to fork
 *     its sequence.
 *   - rand_below() is unbiased. It rejects the few values that would make
 *     some results more likely than others, instead of a plain `% n`.
 *
 * @snippet{trimleft} test_rand.c rand
 *
 * @warning Not for secrets. The sequence is predictable from one output.
 * Use the operating system's generator for keys and tokens.
 * @{
 */

#include "base/base.h"

/** Generator state. Copy it to fork a sequence. */
typedef struct Rand {
    uint64_t state; /**< The seed, advanced by every call. */
} Rand;

/** The next 64 random bits. */
MULE_NODISCARD uint64_t rand_next(Rand *r);
/** A uniform value in [0, @p n).
 *  @pre @p n is not 0. An assert checks it. */
MULE_NODISCARD uint64_t rand_below(Rand *r, uint64_t n);

/** @} */
