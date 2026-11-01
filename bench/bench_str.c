/* Benchmarks for str_append and friends. `make bench` builds this with the
 * release flags, so no sanitizers and full optimisation.
 *
 * Each case builds one string from scratch with `ops` calls, frees it, and is
 * repeated REPS times. The table reports nanoseconds per call for the median
 * and fastest repetition.
 *
 * memcpy into a buffer allocated up front is the floor: no capacity checks and
 * no growth. strcat is the university approach. It rescans the whole string on
 * every call, so it runs at two sizes to show its cost per call growing.
 */
#include "bench.h"

#include "str/str.c"

#include <string.h>

enum { REPS = 7 };

static const StrView chunk16 = { .data = "0123456789abcdef", .len = 16 };
static char chunk1k[1024];

/* --- Cases --------------------------------------------------------------- */

static size_t finish(Str *s) {
    size_t r = s->len + (unsigned char)s->data[s->len / 2];
    str_free(s);
    return r;
}

static size_t run_memcpy16(size_t ops) {
    char *buf = malloc(ops * chunk16.len + 1);
    must(buf != nullptr);
    for (size_t i = 0; i < ops; i++) {
        memcpy(buf + i * chunk16.len, chunk16.data, chunk16.len);
    }
    size_t r = (unsigned char)buf[ops * chunk16.len / 2];
    free(buf);
    return r;
}

static size_t run_append16_reserved(size_t ops) {
    Str s = {};
    must(str_reserve(&s, ops * chunk16.len));
    for (size_t i = 0; i < ops; i++) must(str_append(&s, chunk16));
    return finish(&s);
}

static size_t run_append16(size_t ops) {
    Str s = {};
    for (size_t i = 0; i < ops; i++) must(str_append(&s, chunk16));
    return finish(&s);
}

static size_t run_append1k(size_t ops) {
    Str s = {};
    StrView v = { .data = chunk1k, .len = sizeof chunk1k };
    for (size_t i = 0; i < ops; i++) must(str_append(&s, v));
    return finish(&s);
}

static size_t run_push(size_t ops) {
    Str s = {};
    for (size_t i = 0; i < ops; i++) must(str_push(&s, (char)('a' + i % 26)));
    return finish(&s);
}

static size_t run_appendf(size_t ops) {
    Str s = {};
    for (size_t i = 0; i < ops; i++) must(str_appendf(&s, "%zu,", i));
    return finish(&s);
}

static size_t run_strcat16(size_t ops) {
    char *buf = malloc(ops * chunk16.len + 1);
    must(buf != nullptr);
    buf[0] = '\0';
    for (size_t i = 0; i < ops; i++) strcat(buf, "0123456789abcdef");
    size_t r = strlen(buf);
    free(buf);
    return r;
}

/* --- Driver -------------------------------------------------------------- */

typedef struct Case {
    const char *name;
    size_t (*run)(size_t ops);
    size_t ops;
    size_t bytes_per_op; /* 0 when it varies */
} Case;

int main(void) {
    for (size_t i = 0; i < sizeof chunk1k; i++)
        chunk1k[i] = (char)('a' + i % 26);

    const Case cases[] = {
        { "memcpy 16 B (floor)", run_memcpy16, 1u << 22, 16 },
        { "str_append 16 B, reserved", run_append16_reserved, 1u << 22, 16 },
        { "str_append 16 B", run_append16, 1u << 22, 16 },
        { "str_append 1 KiB", run_append1k, 1u << 16, 1024 },
        { "str_push", run_push, 1u << 24, 1 },
        { "str_appendf \"%zu,\"", run_appendf, 1u << 20, 0 },
        { "strcat 16 B", run_strcat16, 1u << 12, 16 },
        { "strcat 16 B", run_strcat16, 1u << 14, 16 },
    };

    printf("%-28s %10s %12s %12s %10s\n", "case", "calls", "median ns",
           "min ns", "MB/s");
    for (size_t c = 0; c < sizeof cases / sizeof cases[0]; c++) {
        const Case *k = &cases[c];
        double per_op[REPS];
        for (int r = 0; r < REPS; r++) {
            double t0 = now_ns();
            sink += k->run(k->ops);
            per_op[r] = (now_ns() - t0) / (double)k->ops;
        }
        double mid = median(per_op, REPS);

        printf("%-28s %10zu %12.2f %12.2f", k->name, k->ops, mid, per_op[0]);
        if (k->bytes_per_op) {
            printf(" %10.0f\n", (double)k->bytes_per_op / mid * 1e3);
        } else {
            printf(" %10s\n", "-");
        }
    }
    return EXIT_SUCCESS;
}
