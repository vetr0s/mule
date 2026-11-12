/* Str on its own: read a whole file into memory. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "data/pg2554.txt";
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        perror(path);
        return EXIT_FAILURE;
    }

    Str text = { 0 };
    bool ok = str_append_file(&text, f);
    bool read_error = ferror(f);
    fclose(f);
    if (!ok) {
        fprintf(stderr, "%s: %s\n", path,
                read_error ? "read error" : "out of memory");
        return EXIT_FAILURE;
    }

    printf("%s: %zu bytes\n", path, text.len);
    str_free(&text);
    return EXIT_SUCCESS;
}
