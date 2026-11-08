/* Together: one Str owns the file, views walk its lines. */
#include "mem/mem.c"
#include "str/str.c"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    FILE *f = fopen("data/pg2554.txt", "rb");
    if (f == NULL) {
        perror("data/pg2554.txt");
        return EXIT_FAILURE;
    }
    Str text = { 0 };
    bool ok = str_append_file(&text, f);
    fclose(f);
    if (!ok) return EXIT_FAILURE;

    size_t lines = 0, chapters = 0, mentions = 0;
    StrView rest = str_view(&text);
    StrView line;
    while (sv_split_next(&rest, SV("\n"), &line)) {
        line = sv_trim(line); /* the file uses \r\n; trim drops the \r */
        lines++;
        if (sv_starts_with(line, SV("Author:"))) {
            printf("%.*s\n", STR_FMT(line));
        }
        if (sv_starts_with(line, SV("CHAPTER "))) chapters++;

        /* Count every occurrence in the line by sliding past each match. */
        StrView scan = line;
        size_t at;
        while ((at = sv_find(scan, SV("Raskolnikov"))) != SV_NPOS) {
            mentions++;
            scan = sv_slice(scan, at + 1, scan.len);
        }
    }
    printf("%zu lines, %zu chapters, %zu mentions of Raskolnikov\n", lines,
           chapters, mentions);

    str_free(&text); /* every view into it dies here */
    return EXIT_SUCCESS;
}
