/* Content-sensitive benchmarks over real files.
 *
 *   bench_corpus FILE...           one table per file
 *   bench_corpus --scale FILE...   same, then how each workload scales; the
 *                                  files must be one kind of data at
 *                                  increasing sizes
 *
 * `make bench-corpus` runs it on Project Gutenberg books (human language) and
 * the Pizza & Chili source-code and XML corpora (dense symbols). `make
 * bench-scale` runs it on generated 10, 100 and 1000 MiB files.
 *
 * Each file is read once, then built again from scratch in several ways:
 *
 *   ingest         str_append_file on the file
 *   lines          one str_append per line
 *   tokens (scan)  find the tokens but append nothing; the tokenizer's share
 *   tokens         one str_append per token: a run of letters, digits and
 *                  UTF-8 bytes, or a single space or symbol
 *   bytes          one str_push per byte
 *
 * Every rebuilt string must equal the input byte for byte, or the program
 * exits with an error. A run is therefore also a test on real data.
 *
 * The scaling exponent k comes from the smallest and largest file: time grows
 * as size^k. k = 1 is linear; strcat-style rescanning would show k = 2.
 */
#include "bench.h"

#include "mem/mem.c"
#include "str/str.c"
#include "time/time.c"

#include <math.h>
#include <string.h>

enum { REPS = 3, MAX_FILES = 16 };

static bool word_byte[256];

typedef struct Input {
    const char *path;
    StrView text;
} Input;

/* --- Workloads ----------------------------------------------------------- */

static size_t token_end(StrView in, size_t i) {
    if (!word_byte[(unsigned char)in.data[i]]) return i + 1;
    while (i < in.len && word_byte[(unsigned char)in.data[i]]) i++;
    return i;
}

static size_t run_ingest(const Input *in, Str *out) {
    FILE *f = fopen(in->path, "rb");
    must(f != nullptr);
    must(str_append_file(out, f));
    fclose(f);
    return 1;
}

static size_t run_lines(const Input *in, Str *out) {
    StrView t = in->text;
    size_t calls = 0;
    for (size_t i = 0; i < t.len; calls++) {
        const char *nl = memchr(t.data + i, '\n', t.len - i);
        size_t end = nl != nullptr ? (size_t)(nl - t.data) + 1 : t.len;
        must(str_append(out, (StrView){ t.data + i, end - i }));
        i = end;
    }
    return calls;
}

static size_t run_tokens_scan(const Input *in, Str *out) {
    (void)out;
    size_t calls = 0;
    for (size_t i = 0; i < in->text.len; calls++) i = token_end(in->text, i);
    return calls;
}

static size_t run_tokens(const Input *in, Str *out) {
    StrView t = in->text;
    size_t calls = 0;
    for (size_t i = 0; i < t.len; calls++) {
        size_t end = token_end(t, i);
        must(str_append(out, (StrView){ t.data + i, end - i }));
        i = end;
    }
    return calls;
}

static size_t run_bytes(const Input *in, Str *out) {
    for (size_t i = 0; i < in->text.len; i++)
        must(str_push(out, in->text.data[i]));
    return in->text.len;
}

typedef struct Workload {
    const char *name;
    size_t (*run)(const Input *in, Str *out);
    bool rebuilds; /* false when the output is not a copy of the input */
} Workload;

static const Workload workloads[] = {
    { "ingest", run_ingest, true },
    { "lines", run_lines, true },
    { "tokens (scan)", run_tokens_scan, false },
    { "tokens", run_tokens, true },
    { "bytes", run_bytes, true },
};
enum { N_WORKLOADS = sizeof workloads / sizeof workloads[0] };

/* --- Driver -------------------------------------------------------------- */

/* Median time in ns of REPS runs; also checks every run's output. */
static double measure(const Workload *w, const Input *in, size_t *calls) {
    double ns[REPS];
    for (int r = 0; r < REPS; r++) {
        Str out = {};
        double t0 = (double)time_now_ns();
        *calls = w->run(in, &out);
        ns[r] = (double)time_now_ns() - t0;
        sink += *calls + out.len;

        if (w->rebuilds && !str_eq(str_view(&out), in->text)) {
            fprintf(stderr,
                    "%s: %s produced %zu bytes that differ from the "
                    "%zu-byte input\n",
                    in->path, w->name, out.len, in->text.len);
            exit(EXIT_FAILURE);
        }
        str_free(&out);
    }
    return median(ns, REPS);
}

static double mib(size_t bytes) {
    return (double)bytes / 1048576.0;
}

int main(int argc, char **argv) {
    bool scale = argc > 1 && strcmp(argv[1], "--scale") == 0;
    char **paths = argv + 1 + scale;
    int n_files = argc - 1 - scale;
    if (n_files < 1 || n_files > MAX_FILES || (scale && n_files < 2)) {
        fprintf(stderr, "usage: %s [--scale] FILE... (at most %d)\n", argv[0],
                MAX_FILES);
        return EXIT_FAILURE;
    }

    for (int c = 0; c < 256; c++) {
        word_byte[c] = c >= 0x80 ||
                       c == '_' ||
                       (c >= '0' && c <= '9') ||
                       (c >= 'a' && c <= 'z') ||
                       (c >= 'A' && c <= 'Z');
    }

    size_t sizes[MAX_FILES];
    double ns[MAX_FILES][N_WORKLOADS];

    for (int f = 0; f < n_files; f++) {
        Input in = { .path = paths[f] };
        Str text = {};
        FILE *fp = fopen(in.path, "rb");
        if (fp == nullptr) {
            perror(in.path);
            return EXIT_FAILURE;
        }
        must(str_append_file(&text, fp));
        fclose(fp);
        in.text = str_view(&text);
        sizes[f] = text.len;

        printf("\n%s  (%.1f MiB)\n", in.path, mib(text.len));
        printf("  %-14s %12s %11s %12s %10s\n", "workload", "calls", "B/call",
               "ns/call", "MB/s");
        for (int w = 0; w < N_WORKLOADS; w++) {
            size_t calls = 0;
            ns[f][w] = measure(&workloads[w], &in, &calls);
            printf("  %-14s %12zu %11.1f %12.2f %10.0f\n", workloads[w].name,
                   calls, (double)text.len / (double)calls,
                   ns[f][w] / (double)calls, (double)text.len / ns[f][w] * 1e3);
        }
        str_free(&text);
    }

    if (scale) {
        int last = n_files - 1;
        printf("\nscaling, %.0f MiB to %.0f MiB (time ~ size^k)\n",
               mib(sizes[0]), mib(sizes[last]));
        printf("  %-14s", "ns/byte at");
        for (int f = 0; f < n_files; f++) printf(" %7.0fM", mib(sizes[f]));
        printf(" %7s\n", "k");
        for (int w = 0; w < N_WORKLOADS; w++) {
            printf("  %-14s", workloads[w].name);
            for (int f = 0; f < n_files; f++) {
                printf(" %8.3f", ns[f][w] / (double)sizes[f]);
            }
            double k = log(ns[last][w] / ns[0][w]) /
                       log((double)sizes[last] / (double)sizes[0]);
            printf(" %7.2f\n", k);
        }
    }
    return EXIT_SUCCESS;
}
