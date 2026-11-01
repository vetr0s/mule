/* Str on its own: clear and refill one buffer instead of allocating per line.
 */
#include "str.c"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    Str line = {};
    for (int part = 1; part <= 3; part++) {
        str_clear(&line);
        if (!str_appendf(&line, "PART %d", part)) {
            str_free(&line);
            return EXIT_FAILURE;
        }
        printf("%-8s capacity %zu, data %p\n", str_cstr(&line), line.capacity,
               (void *)line.data);
    }
    str_free(&line);
    return EXIT_SUCCESS;
}
