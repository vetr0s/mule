/* mem: one arena per request, so a whole batch of strings frees at once. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    Arena arena = { 0 };
    const Allocator *a = arena_allocator(&arena);
    const char *names[] = { "Raskolnikov", "Sonya", "Dunya" };

    for (int request = 1; request <= 2; request++) {
        /* A mark rewinds everything allocated after it in one call. */
        ArenaMark mark = arena_mark(&arena);
        Str greeting = { .alloc = a };
        for (size_t i = 0; i < 3; i++) {
            if (!str_appendf(&greeting, "%s%s", i ? ", " : "", names[i])) {
                arena_free(&arena);
                return EXIT_FAILURE;
            }
        }
        printf("request %d: hello %s\n", request, str_cstr(&greeting));
        arena_restore(&arena, mark);
    }
    arena_free(&arena);
    return EXIT_SUCCESS;
}
