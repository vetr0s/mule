/* Unity build: every implementation file is included here, so the whole
 * program is one translation unit (fast builds, whole-program optimisation). */
#include "str.c"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    Str s = {};
    if (!str_from_cstr(&s, "Hello") ||
        !str_append(&s, SV(", ")) ||
        !str_appendf(&s, "World #%d", 23) ||
        !str_push(&s, '!')) {
        fputs("out of memory\n", stderr);
        str_free(&s);
        return EXIT_FAILURE;
    }

    printf("%.*s (len=%zu, cap=%zu)\n", STR_FMT(s), s.len, s.capacity);
    str_free(&s);
    return EXIT_SUCCESS;
}
