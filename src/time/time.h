#pragma once
/**
 * @defgroup mod_time Time
 * @brief A monotonic clock for measuring how long things take.
 *
 * time_now_ns() reads `CLOCK_MONOTONIC`. It never goes backwards, so the
 * difference between two readings is a duration even when the wall clock
 * changes. It does not tell the time of day.
 *
 * @snippet{trimleft} test_time.c time
 * @{
 */

#include "base/base.h"

/** Nanoseconds on a monotonic clock. It never goes backwards, but its zero is
 *  arbitrary, so only the difference between two readings means anything. */
MULE_NODISCARD uint64_t time_now_ns(void);

/** @} */
