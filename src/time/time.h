#pragma once
/**
 * @file
 * @brief Time: clocks for measuring how long things take.
 *
 * @snippet{trimleft} test_time.c time
 */

#include "base/base.h"

/** Nanoseconds on a monotonic clock. It never goes backwards, but its zero is
 *  arbitrary, so only the difference between two readings means anything. */
MULE_NODISCARD uint64_t time_now_ns(void);
