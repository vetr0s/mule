/* StrView on its own: inspect and slice text without allocating. */
#include "mem/mem.c"
#include "str/str.c"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    StrView header = SV("  Title: Crime and Punishment\r\n");
    StrView line = sv_trim(header);
    printf("[%.*s]\n", STR_FMT(line));

    if (sv_starts_with(line, SV("Title:"))) {
        size_t colon = sv_find_byte(line, ':');
        StrView key = sv_slice(line, 0, colon);
        StrView value = sv_trim(sv_slice(line, colon + 1, line.len));
        printf("key [%.*s] value [%.*s]\n", STR_FMT(key), STR_FMT(value));

        /* Wrong: %s runs on to the next NUL, past the end of the view. */
        printf("%%s on key prints [%s]\n", key.data);
    }

    size_t at = sv_find(line, SV("Punishment"));
    if (at != SV_NPOS) printf("Punishment starts at byte %zu\n", at);
    printf("ends with \"ment\": %d\n", sv_ends_with(line, SV("ment")));

    /* argv strings already exist, so a view just borrows them. */
    for (int i = 1; i < argc; i++) {
        StrView arg = sv_from_cstr(argv[i]);
        if (str_eq(arg, SV("--verbose"))) puts("verbose on");
    }
    return EXIT_SUCCESS;
}
