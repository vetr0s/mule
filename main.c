/* A word-frequency counter that shows MULE in use.
 *
 *   .build/debug/main [file]     reads stdin when no file is given
 *
 * The file is read into a Str that lives in an arena and is folded to lower
 * case in place, with UTF-8 quotes and dashes blanked. Words are StrViews into
 * that one buffer, so the map that counts them borrows its keys and copies no
 * text. The map's entries and index live in the same arena, and one arena_free
 * releases it all.
 *
 * It uses the single-header distribution, dist/mule.h, the way any project
 * would. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TOP = 10 };

typedef struct WordCount {
    StrView key;
    size_t count;
} WordCount;
typedef Map(WordCount) WordMap;

/* Most frequent first, and ties in byte order, so the output is stable. */
static int by_count(const void *a, const void *b) {
    const WordCount *x = a, *y = b;
    if (x->count != y->count) return x->count < y->count ? 1 : -1;
    return sv_cmp(x->key, y->key);
}

/* Fold ASCII to lower case, and blank the UTF-8 dashes, quotes and ellipsis,
 * E2 80 90 to E2 80 A6. ’ stays, because it is the apostrophe in "don’t". */
static void fold(Str *text) {
    unsigned char *p = (unsigned char *)text->data;
    size_t n = text->len;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == 0xE2 &&
            i + 2 < n &&
            p[i + 1] == 0x80 &&
            p[i + 2] >= 0x90 &&
            p[i + 2] <= 0xA6 &&
            p[i + 2] != 0x99) {
            memset(p + i, ' ', 3);
            i += 2;
        } else {
            p[i] = (unsigned char)tolower(p[i]);
        }
    }
}

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
    fold(&text);

    WordMap counts = { .alloc = arena_allocator(&arena) };
    size_t words = 0;
    StrView rest = str_view(&text), word;
    while (sv_token_next(&rest, SV(" \t\n\v\f\r.,;:!?\"()[]{}<>*_-"), &word)) {
        size_t i;
        if (!map_put(&counts, word, &i)) {
            fprintf(stderr, "%s: out of memory\n", name);
            arena_free(&arena);
            return EXIT_FAILURE;
        }
        counts.data[i].count++;
        words++;
    }
    /* Sorting reorders the entries, so the map takes no lookups after it. */
    if (counts.len > 0)
        qsort(counts.data, counts.len, sizeof *counts.data, by_count);

    bool color = log_color_enabled(stdout);
    print_count(color, "words", words);
    print_count(color, "unique", counts.len);
    for (size_t i = 0; i < counts.len && i < TOP; i++)
        printf("%8zu %.*s\n", counts.data[i].count,
               STR_FMT(counts.data[i].key));

    arena_free(&arena);
    return EXIT_SUCCESS;
}
