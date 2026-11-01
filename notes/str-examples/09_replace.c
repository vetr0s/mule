/* Together: write your own function with views in and a Str out. */
#include "str/str.c"

#include <stdio.h>
#include <stdlib.h>

/* Append `text` to `out` with every `from` replaced by `to`. */
static bool replace_all(Str *out, StrView text, StrView from, StrView to) {
    size_t at;
    while ((at = sv_find(text, from)) != SV_NPOS) {
        if (!str_append(out, sv_slice(text, 0, at)) || !str_append(out, to)) {
            return false;
        }
        text = sv_slice(text, at + from.len, text.len);
    }
    return str_append(out, text);
}

int main(void) {
    Str out = {};
    if (!replace_all(&out, SV("Rodya went out. Rodya came back."), SV("Rodya"),
                     SV("Raskolnikov"))) {
        str_free(&out);
        return EXIT_FAILURE;
    }
    printf("%.*s\n", STR_FMT(out));

    /* Appending a Str to itself is safe: str_append handles the overlap. */
    if (!str_append(&out, str_view(&out))) {
        str_free(&out);
        return EXIT_FAILURE;
    }
    printf("%zu bytes after doubling\n", out.len);
    str_free(&out);
    return EXIT_SUCCESS;
}
