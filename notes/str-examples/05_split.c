/* StrView on its own: split fields and tokens. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    /* Split keeps empty fields: the right tool for CSV-like data. */
    StrView rest = SV("Raskolnikov,Sonya,,Dunya,");
    StrView field;
    while (sv_split_next(&rest, SV(","), &field)) {
        printf("field [%.*s]\n", STR_FMT(field));
    }

    /* Token skips runs of delimiters: the right tool for words. */
    rest = SV("  Pale,  feverish   and  restless. ");
    StrView word;
    while (sv_token_next(&rest, SV(" ,."), &word)) {
        printf("word  [%.*s]\n", STR_FMT(word));
    }
    return EXIT_SUCCESS;
}
