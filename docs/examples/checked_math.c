/* base: size arithmetic that cannot wrap, with ckd_add and ckd_mul. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* The bytes for `count` records of `size` bytes plus a header, or false
 * when that does not fit in a size_t. */
static bool table_bytes(size_t count, size_t size, size_t *out) {
    size_t body;
    return !ckd_mul(&body, count, size) && !ckd_add(out, body, 64);
}

int main(void) {
    size_t n;
    if (table_bytes(1000, 48, &n)) printf("1000 records: %zu bytes\n", n);
    if (!table_bytes(SIZE_MAX / 2, 48, &n))
        printf("SIZE_MAX / 2 records: too big, caught before malloc\n");
    return EXIT_SUCCESS;
}
