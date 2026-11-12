/* map: tally items by name, keep their first-seen order, and remove one. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct Item {
    StrView key; /* every map entry starts with its key */
    int count;
} Item;
typedef Map(Item) Inventory;

int main(void) {
    Inventory inv = { 0 };
    StrView rest = SV("axe rope lamp rope rope axe bread"), word;
    while (sv_token_next(&rest, SV(" "), &word)) {
        size_t i;
        if (!map_put(&inv, word, &i)) {
            map_free(&inv);
            return EXIT_FAILURE;
        }
        inv.data[i].count++;
    }
    if (map_remove(&inv, SV("lamp"))) puts("sold the lamp");

    for (size_t i = 0; i < inv.len; i++)
        printf("%-6.*s %d\n", STR_FMT(inv.data[i].key), inv.data[i].count);
    size_t i;
    printf("lamp: %s\n", map_find(&inv, SV("lamp"), &i) ? "kept" : "gone");
    map_free(&inv);
    return EXIT_SUCCESS;
}
