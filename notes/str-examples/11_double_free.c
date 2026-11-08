/* Bug on purpose: copying a Str struct makes two owners of one buffer. */
#include "mem/mem.c"
#include "str/str.c"

#include <stdlib.h>

int main(void) {
    Str a = { 0 };
    if (!str_append_cstr(&a, "Dunya")) return EXIT_FAILURE;
    Str b = a; /* copies the pointer, not the bytes */
    str_free(&a);
    str_free(&b); /* frees the same buffer again */
    return EXIT_SUCCESS;
}
