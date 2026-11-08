/* Bug on purpose: a view kept across a call that reallocates. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    Str s = { 0 };
    if (!str_append_cstr(&s, "Sonya")) return EXIT_FAILURE;
    StrView name = str_view(&s);

    /* 15 bytes fit; this append needs more, so the buffer moves. */
    if (!str_append_cstr(&s, " Semyonovna Marmeladova")) return EXIT_FAILURE;

    printf("%.*s\n", STR_FMT(name)); /* reads freed memory */
    str_free(&s);
    return EXIT_SUCCESS;
}
