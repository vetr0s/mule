/* time: measure how long a piece of work takes with a monotonic clock. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    Str s = { 0 };
    uint64_t start = time_now_ns();
    for (int i = 0; i < 100000; i++) {
        if (!str_appendf(&s, "%d,", i)) {
            str_free(&s);
            return EXIT_FAILURE;
        }
    }
    uint64_t took = time_now_ns() - start;
    printf("appended %zu bytes in %.2f ms\n", s.len, (double)took / 1e6);
    str_free(&s);
    return EXIT_SUCCESS;
}
