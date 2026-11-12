/* The word-frequency pipeline of main.c, split into its phases and timed on
 * real files. `./build bench-map` runs it on the corpus.
 *
 *   fold     ASCII to lower case, in place
 *   tokens   sv_token_next over the folded text, with main.c's delimiters
 *   hash     the map's hash of every word, and nothing else
 *   count    tokens, then map_put and a count per word, as main.c does
 *   reserved count, into a map reserved for every unique word up front
 *   find     map_find of every word in the finished map, which all hit
 *   sort     qsort of the entries by count, as main.c does
 *
 * Each phase reports the median of REPS runs in ns per word, except sort,
 * which reports ns per unique word. The index section shows how far each
 * entry's slot sits from where its hash points, which is the extra work a
 * lookup does when keys collide.
 */
#include "bench.h"

#include "arr/arr.c"
#include "map/map.c"
#include "mem/mem.c"
#include "str/str.c"
#include "time/time.c"

#include <ctype.h>
#include <string.h>

enum { REPS = 3 };

typedef struct WordCount {
    StrView key;
    size_t count;
} WordCount;
typedef Map(WordCount) WordMap;

#define DELIMS " \t\n\v\f\r.,;:!?\"()[]{}<>*_-"

static int by_count(const void *a, const void *b) {
    const WordCount *x = a, *y = b;
    if (x->count != y->count) return x->count < y->count ? 1 : -1;
    return sv_cmp(x->key, y->key);
}

static size_t count_words(StrView text, WordMap *m) {
    size_t words = 0;
    StrView rest = text, word;
    while (sv_token_next(&rest, SV(DELIMS), &word)) {
        size_t i;
        must(map_put(m, word, &i));
        m->data[i].count++;
        words++;
    }
    return words;
}

/* Median ns of REPS runs of the statements after `out`. */
#define TIME(out, ...)                                                         \
    do {                                                                       \
        double ns_[REPS];                                                      \
        for (int r = 0; r < REPS; r++) {                                       \
            double t0_ = (double)time_now_ns();                                \
            __VA_ARGS__;                                                       \
            ns_[r] = (double)time_now_ns() - t0_;                              \
        }                                                                      \
        (out) = median(ns_, REPS);                                             \
    } while (0)

static void row(const char *name, double ns, size_t per, const char *unit) {
    printf("  %-10s %10.2f ns/%s %10.0f ms\n", name, ns / (double)per, unit,
           ns / 1e6);
}

static void bench_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        perror(path);
        exit(EXIT_FAILURE);
    }
    Str text = { 0 };
    must(str_append_file(&text, f));
    fclose(f);
    printf("\n%s  (%.1f MiB)\n", path, (double)text.len / 1048576.0);

    double ns;
    TIME(ns, for (size_t i = 0; i < text.len; i++)
                 text.data[i] = (char)tolower((unsigned char)text.data[i]));
    double fold_ns = ns;
    StrView all = str_view(&text);

    size_t words = 0;
    TIME(ns, {
        StrView rest = all, word;
        words = 0;
        while (sv_token_next(&rest, SV(DELIMS), &word)) words++;
        sink += words;
    });
    double tokens_ns = ns;

    TIME(ns, {
        StrView rest = all, word;
        uint32_t h = 0;
        while (sv_token_next(&rest, SV(DELIMS), &word)) h ^= map_hash_(word);
        sink += h;
    });
    double hash_ns = ns;

    WordMap m = { 0 };
    TIME(ns, {
        map_free(&m);
        sink += count_words(all, &m);
    });
    double count_ns = ns;
    size_t unique = m.len;

    WordMap reserved = { 0 };
    TIME(ns, {
        map_free(&reserved);
        must(map_reserve(&reserved, unique));
        sink += count_words(all, &reserved);
    });
    double reserved_ns = ns;
    map_free(&reserved);

    TIME(ns, {
        StrView rest = all, word;
        size_t i, hits = 0;
        while (sv_token_next(&rest, SV(DELIMS), &word))
            hits += map_find(&m, word, &i);
        sink += hits;
    });
    double find_ns = ns;

    WordCount *copy = malloc(unique * sizeof *copy + 1);
    must(copy != NULL);
    TIME(ns, {
        memcpy(copy, m.data, unique * sizeof *copy);
        qsort(copy, unique, sizeof *copy, by_count);
        sink += copy[0].count;
    });
    double sort_ns = ns;
    free(copy);

    printf("  %zu words, %zu unique\n", words, unique);
    row("fold", fold_ns, words, "word");
    row("tokens", tokens_ns, words, "word");
    row("hash", hash_ns, words, "word");
    row("count", count_ns, words, "word");
    row("reserved", reserved_ns, words, "word");
    row("find", find_ns, words, "word");
    row("sort", sort_ns, unique, "uniq");

    size_t mask = m.slot_count - 1, total = 0, longest = 0;
    for (size_t p = 0; p < m.slot_count; p++) {
        uint64_t s = m.slots[p];
        if (s == 0) continue;
        size_t d = (p - ((uint32_t)(s >> 32) & mask)) & mask;
        total += d;
        if (d > longest) longest = d;
    }
    printf("  index: %zu slots, %.0f%% full, mean distance %.2f, max %zu, "
           "%.1f MiB with entries\n",
           m.slot_count, 100.0 * (double)unique / (double)m.slot_count,
           (double)total / (double)unique, longest,
           (double)(m.slot_count * 8 + m.capacity * sizeof *m.data) /
               1048576.0);
    map_free(&m);
    str_free(&text);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s FILE...\n", argv[0]);
        return EXIT_FAILURE;
    }
    for (int i = 1; i < argc; i++) bench_file(argv[i]);
    return EXIT_SUCCESS;
}
