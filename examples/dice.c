/* rand: a seeded generator, so a run can be replayed from its seed. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    Rand r = { .state = 2026 };
    int totals[13] = { 0 };
    for (int roll = 0; roll < 36000; roll++)
        totals[2 + rand_below(&r, 6) + rand_below(&r, 6)]++;

    puts("two dice, 36000 rolls, seed 2026:");
    for (int sum = 2; sum <= 12; sum++)
        printf("%3d %5d %.*s\n", sum, totals[sum], totals[sum] / 200,
               "################################");
    return EXIT_SUCCESS;
}
