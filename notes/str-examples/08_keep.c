/* Together: copy a view into its own Str when it must outlive its source. */
#include "mem/mem.c"
#include "str/str.c"

#include <stdio.h>
#include <stdlib.h>

typedef struct Book {
    Str title;
    Str author;
} Book;

static void book_free(Book *b) {
    str_free(&b->title);
    str_free(&b->author);
}

/* Fill `b` from a Gutenberg header. Views in, owned copies out. */
static bool book_parse(Book *b, StrView text) {
    StrView line;
    while (sv_split_next(&text, SV("\n"), &line)) {
        line = sv_trim(line);
        if (sv_starts_with(line, SV("Title: "))) {
            str_clear(&b->title);
            if (!str_append(&b->title, sv_slice(line, 7, line.len)))
                return false;
        } else if (sv_starts_with(line, SV("Author: "))) {
            str_clear(&b->author);
            if (!str_append(&b->author, sv_slice(line, 8, line.len)))
                return false;
        }
    }
    return true;
}

int main(void) {
    FILE *f = fopen("data/pg2554.txt", "rb");
    if (f == nullptr) return EXIT_FAILURE;
    Str text = {};
    bool ok = str_append_file(&text, f);
    fclose(f);
    if (!ok) return EXIT_FAILURE;

    Book book = {};
    ok = book_parse(&book, str_view(&text));
    str_free(&text); /* the 1.2 MB buffer goes; the book keeps its copies */
    if (!ok) {
        book_free(&book);
        return EXIT_FAILURE;
    }

    printf("%s by %s\n", str_cstr(&book.title), str_cstr(&book.author));
    book_free(&book);
    return EXIT_SUCCESS;
}
