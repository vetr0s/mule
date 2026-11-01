/* Helpers shared by the benchmarks in bench/. Include this first: it defines
 * _POSIX_C_SOURCE, which only takes effect before any system header. */
#pragma once

#define _POSIX_C_SOURCE 200809L /* clock_gettime under -std=c23 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Every benchmark feeds its results here so the optimiser cannot drop work. */
[[maybe_unused]] static volatile size_t sink;

[[maybe_unused]] static void must(bool ok) {
    if (!ok) {
        fputs("out of memory\n", stderr);
        exit(EXIT_FAILURE);
    }
}

[[maybe_unused]] static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* Sorts `xs` in place and returns its median. */
[[maybe_unused]] static double median(double *xs, size_t n) {
    qsort(xs, n, sizeof xs[0], cmp_double);
    return xs[n / 2];
}
