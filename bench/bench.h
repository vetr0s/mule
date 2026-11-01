/* Helpers shared by the benchmarks in bench/. Include this first: it pulls in
 * base.h, whose feature macros only take effect before any system header. */
#pragma once

#include "base/base.h" /* first: it sets the POSIX feature macros */

#include <stdio.h>
#include <stdlib.h>

/* Every benchmark feeds its results here so the optimiser cannot drop work. */
[[maybe_unused]] static volatile size_t sink;

[[maybe_unused]] static void must(bool ok) {
    if (!ok) {
        fputs("out of memory\n", stderr);
        exit(EXIT_FAILURE);
    }
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
