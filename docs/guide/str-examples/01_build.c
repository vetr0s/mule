/* Str on its own: build a string, read it, free it. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    Str s = { 0 };

    /* A failed call leaves `s` unchanged, so && stops at the first failure. */
    bool ok = str_append_cstr(&s, "Rodion") &&
              str_push(&s, ' ') &&
              str_append(&s, SV("Raskolnikov")) &&
              str_appendf(&s, ", former student, age %d", 23);
    if (!ok) {
        str_free(&s);
        return EXIT_FAILURE;
    }

    printf("%.*s\n", STR_FMT(s));
    printf("len %zu, capacity %zu\n", s.len, s.capacity);
    puts(str_cstr(&s));

    str_free(&s);
    return EXIT_SUCCESS;
}
