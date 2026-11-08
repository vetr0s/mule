/* A small word counter that shows MULE in use.
 *
 *   build/debug/main [file]      reads stdin when no file is given
 *
 * The file is read into a Str that lives in an arena. Lines and words are
 * StrViews into that one buffer, so counting allocates nothing more, and one
 * arena_free releases it all.
 *
 * It uses the single-header distribution, dist/mule.h, the way any project
 * would. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

static void print_count(bool color, const char *label, size_t n) {
    if (color)
        printf(LOG_GREEN "%-8s" LOG_RESET " %zu\n", label, n);
    else
        printf("%-8s %zu\n", label, n);
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "usage: %s [file]\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *name = argc == 2 ? argv[1] : "stdin";
    FILE *f = argc == 2 ? fopen(name, "rb") : stdin;
    if (f == NULL) {
        perror(name);
        return EXIT_FAILURE;
    }

    Arena arena = { 0 };
    Str text = { .alloc = arena_allocator(&arena) };
    bool ok = str_append_file(&text, f);
    bool read_error = ferror(f);
    if (f != stdin) fclose(f);
    if (!ok) {
        fprintf(stderr, "%s: %s\n", name,
                read_error ? "read error" : "out of memory");
        arena_free(&arena);
        return EXIT_FAILURE;
    }

    size_t lines = 0, words = 0, longest = 0;
    StrView rest = str_view(&text), line, word;
    while (sv_split_next(&rest, SV("\n"), &line)) {
        lines++;
        if (sv_ends_with(line, SV("\r")))
            line = sv_slice(line, 0, line.len - 1);
        if (line.len > longest) longest = line.len;
        while (sv_token_next(&line, SV(" \t\v\f\r"), &word)) words++;
    }

    bool color = log_color_enabled(stdout);
    print_count(color, "lines", lines);
    print_count(color, "words", words);
    print_count(color, "bytes", text.len);
    print_count(color, "longest", longest);

    arena_free(&arena);
    return EXIT_SUCCESS;
}
