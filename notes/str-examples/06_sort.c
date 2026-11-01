/* StrView on its own: sort an array of views with qsort. */
#include "str/str.c"

#include <stdio.h>
#include <stdlib.h>

static int cmp_views(const void *a, const void *b) {
    return sv_cmp(*(const StrView *)a, *(const StrView *)b);
}

int main(void) {
    StrView names[] = { SV("Sonya"), SV("Dunya"), SV("Razumikhin"),
                        SV("Raskolnikov"), SV("Porfiry") };
    size_t n = sizeof names / sizeof names[0];

    qsort(names, n, sizeof names[0], cmp_views);
    for (size_t i = 0; i < n; i++) printf("%.*s\n", STR_FMT(names[i]));
    return EXIT_SUCCESS;
}
