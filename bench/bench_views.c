/* The StrView search and comparison functions against their string.h
 * counterparts, on the same files. `make bench-corpus` runs it.
 *
 *   find '\n'   sv_find_byte vs strchr, counting newlines
 *   find "the"  sv_find vs strstr, counting non-overlapping matches
 *   tokens      sv_token_next vs strtok, splitting on whitespace
 *   sort lines  qsort with sv_cmp vs qsort with strcmp
 *
 * Each pair must produce the same result, or the program exits with an
 * error. Setup runs before every timed repetition and is not timed: strtok
 * writes NULs into its input, so it needs a fresh copy, and qsort needs the
 * lines back in file order.
 */
#include "bench.h"

#include "mem/mem.c"
#include "str/str.c"
#include "time/time.c"

#include <string.h>

enum { REPS = 5 };

static const char NEEDLE[] = "the";
static const char DELIMS[] = " \t\r\n";

typedef struct Corpus {
    StrView text;   /* NUL-terminated, so string.h can read it as-is */
    char *scratch;  /* strtok's copy of the text */
    char *line_buf; /* the text with each '\n' replaced by a NUL */
    size_t n_lines;
    StrView *lines;      /* file order */
    char **line_cstrs;   /* file order, pointing into line_buf */
    StrView *sorted;     /* sorted by sv_cmp */
    char **sorted_cstrs; /* sorted by strcmp */
} Corpus;

/* --- Pairs --------------------------------------------------------------- */

static size_t ours_find_byte(Corpus *c) {
    size_t n = 0;
    StrView rest = c->text;
    for (size_t at; (at = sv_find_byte(rest, '\n')) != SV_NPOS; n++) {
        rest = sv_slice(rest, at + 1, rest.len);
    }
    return n;
}

static size_t theirs_find_byte(Corpus *c) {
    size_t n = 0;
    for (const char *p = strchr(c->text.data, '\n'); p;
         p = strchr(p + 1, '\n')) {
        n++;
    }
    return n;
}

static size_t ours_find_text(Corpus *c) {
    size_t n = 0;
    StrView rest = c->text, needle = SV(NEEDLE);
    for (size_t at; (at = sv_find(rest, needle)) != SV_NPOS; n++) {
        rest = sv_slice(rest, at + needle.len, rest.len);
    }
    return n;
}

static size_t theirs_find_text(Corpus *c) {
    size_t n = 0;
    const size_t len = sizeof NEEDLE - 1;
    for (const char *p = strstr(c->text.data, NEEDLE); p;
         p = strstr(p + len, NEEDLE)) {
        n++;
    }
    return n;
}

static void prepare_scratch(Corpus *c) {
    memcpy(c->scratch, c->text.data, c->text.len + 1);
}

static size_t ours_tokens(Corpus *c) {
    size_t n = 0;
    StrView rest = c->text, tok;
    while (sv_token_next(&rest, SV(DELIMS), &tok)) n++;
    return n;
}

static size_t theirs_tokens(Corpus *c) {
    size_t n = 0;
    for (char *t = strtok(c->scratch, DELIMS); t; t = strtok(nullptr, DELIMS)) {
        n++;
    }
    return n;
}

static void prepare_views(Corpus *c) {
    memcpy(c->sorted, c->lines, c->n_lines * sizeof c->lines[0]);
}

static void prepare_cstrs(Corpus *c) {
    memcpy(c->sorted_cstrs, c->line_cstrs,
           c->n_lines * sizeof c->line_cstrs[0]);
}

static int cmp_view(const void *a, const void *b) {
    return sv_cmp(*(const StrView *)a, *(const StrView *)b);
}

static int cmp_cstr(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static size_t ours_sort(Corpus *c) {
    qsort(c->sorted, c->n_lines, sizeof c->sorted[0], cmp_view);
    return c->n_lines;
}

static size_t theirs_sort(Corpus *c) {
    qsort(c->sorted_cstrs, c->n_lines, sizeof c->sorted_cstrs[0], cmp_cstr);
    return c->n_lines;
}

/* Both sorts are left in place by their last run; they must match line by
 * line. */
static bool sorts_agree(const Corpus *c) {
    for (size_t i = 0; i < c->n_lines; i++) {
        if (!str_eq(c->sorted[i], sv_from_cstr(c->sorted_cstrs[i])))
            return false;
    }
    return true;
}

/* One side of a pair. Each side resets only its own state, so the last run of
 * one side survives the other side's runs for the agreement check. */
typedef struct Side {
    void (*prepare)(Corpus *c); /* untimed, before every run; may be null */
    size_t (*run)(Corpus *c);
} Side;

typedef struct Pair {
    const char *name;
    Side ours, theirs;
    bool (*agree)(const Corpus *c); /* beyond equal results; may be null */
} Pair;

static const Pair pairs[] = {
    { "find '\\n'",
      { nullptr, ours_find_byte },
      { nullptr, theirs_find_byte },
      nullptr },
    { "find \"the\"",
      { nullptr, ours_find_text },
      { nullptr, theirs_find_text },
      nullptr },
    { "tokens",
      { nullptr, ours_tokens },
      { prepare_scratch, theirs_tokens },
      nullptr },
    { "sort lines",
      { prepare_views, ours_sort },
      { prepare_cstrs, theirs_sort },
      sorts_agree },
};

/* --- Driver -------------------------------------------------------------- */

/* Median ms over REPS runs; `result` gets the last run's return value. */
static double time_ms(const Side *side, Corpus *c, size_t *result) {
    double ms[REPS];
    for (int r = 0; r < REPS; r++) {
        if (side->prepare) side->prepare(c);
        double t0 = (double)time_now_ns();
        *result = side->run(c);
        ms[r] = ((double)time_now_ns() - t0) / 1e6;
        sink += *result;
    }
    return median(ms, REPS);
}

/* Split the text into lines once, as views and as NUL-terminated copies. */
static void load_lines(Corpus *c) {
    size_t cap = ours_find_byte(c) + 1;
    c->lines = malloc(cap * sizeof c->lines[0]);
    c->line_cstrs = malloc(cap * sizeof c->line_cstrs[0]);
    c->sorted = malloc(cap * sizeof c->sorted[0]);
    c->sorted_cstrs = malloc(cap * sizeof c->sorted_cstrs[0]);
    c->line_buf = malloc(c->text.len + 1);
    c->scratch = malloc(c->text.len + 1);
    must(c->lines &&
         c->line_cstrs &&
         c->sorted &&
         c->sorted_cstrs &&
         c->line_buf &&
         c->scratch);
    memcpy(c->line_buf, c->text.data, c->text.len + 1);

    StrView rest = c->text, line;
    c->n_lines = 0;
    while (sv_split_next(&rest, SV("\n"), &line)) {
        size_t off = (size_t)(line.data - c->text.data);
        c->line_buf[off + line.len] = '\0';
        c->lines[c->n_lines] = line;
        c->line_cstrs[c->n_lines] = c->line_buf + off;
        c->n_lines++;
    }
}

static void free_corpus(Corpus *c) {
    free(c->lines);
    free(c->line_cstrs);
    free(c->sorted);
    free(c->sorted_cstrs);
    free(c->line_buf);
    free(c->scratch);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s FILE...\n", argv[0]);
        return EXIT_FAILURE;
    }

    for (int f = 1; f < argc; f++) {
        Str text = {};
        FILE *fp = fopen(argv[f], "rb");
        if (fp == nullptr) {
            perror(argv[f]);
            return EXIT_FAILURE;
        }
        must(str_append_file(&text, fp));
        fclose(fp);
        if (sv_find_byte(str_view(&text), '\0') != SV_NPOS) {
            fprintf(stderr,
                    "%s: contains NUL bytes, which string.h cannot "
                    "see past\n",
                    argv[f]);
            return EXIT_FAILURE;
        }

        Corpus c = { .text = str_view(&text) };
        load_lines(&c);

        printf("\n%s  (%.1f MiB, %zu lines)\n", argv[f],
               (double)text.len / 1048576.0, c.n_lines);
        printf("  %-12s %10s %12s %12s %10s\n", "task", "result", "StrView ms",
               "string.h ms", "speedup");
        for (size_t i = 0; i < sizeof pairs / sizeof pairs[0]; i++) {
            const Pair *p = &pairs[i];
            size_t ours = 0, theirs = 0;
            double ours_ms = time_ms(&p->ours, &c, &ours);
            double theirs_ms = time_ms(&p->theirs, &c, &theirs);
            if (ours != theirs || (p->agree && !p->agree(&c))) {
                fprintf(stderr, "%s: %s disagrees: StrView %zu, string.h %zu\n",
                        argv[f], p->name, ours, theirs);
                return EXIT_FAILURE;
            }
            printf("  %-12s %10zu %12.2f %12.2f %9.2fx\n", p->name, ours,
                   ours_ms, theirs_ms, theirs_ms / ours_ms);
        }
        free_corpus(&c);
        str_free(&text);
    }
    return EXIT_SUCCESS;
}
