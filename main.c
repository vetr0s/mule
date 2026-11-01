/* Unity build: every implementation file is included here, so the whole
 * program is one translation unit (fast builds, whole-program optimisation). */
#include "str.c"
#include "str.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    printf("argc:%d\nargv[0]:%s\n", argc, *argv);
    Str userName = {};

    bool res = str_append_cstr(&userName, "Nathan Tebbs");
    if (res == false) {
        perror("Failed to append str!\n");
        return EXIT_FAILURE;
    }

    printf("cap: %zu\nlen: %zu\ncontents: '%.*s'\n", userName.capacity,
           userName.len, STR_FMT(userName));

    str_free(&userName);
    return EXIT_SUCCESS;
}
