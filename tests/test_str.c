/* Tests for src/str.c.
 *
 * Pairs of comments holding a name in square brackets mark regions that
 * src/str.h pulls into the Doxygen reference with @snippet. Every documented
 * example is therefore compiled and run here. Doxygen matches the bracketed
 * name anywhere in this file, so never write one outside a marker. Renaming a
 * marker without updating str.h makes `make docs` fail.
 */
#include "test.h"

#include "str/str.c"

#include <stdint.h>

/* Compare a StrView against a string literal, embedded NULs included. */
#define CHECK_SV(v, lit) CHECK_EQ_MEM((v).data, (v).len, lit, sizeof(lit) - 1)

/* --- Lifetime ------------------------------------------------------------ */

TEST(zero_init_is_empty) {
    Str s = {};
    CHECK_EQ_INT(s.len, 0);
    CHECK_EQ_STR(str_cstr(&s), "");
    CHECK_SV(str_view(&s), "");
    str_clear(&s);
    CHECK(str_shrink_to_fit(&s));
    CHECK(s.data == nullptr);
    str_free(&s);
}

TEST(from_and_append) {
    /* [build] */
    Str s = {};
    CHECK(str_from_cstr(&s, "foo"));
    CHECK(str_append(&s, SV("bar")));
    CHECK(str_push(&s, '!'));
    CHECK_EQ_STR(str_cstr(&s), "foobar!");
    str_free(&s);
    /* [build] */
    CHECK(s.data == nullptr);
}

TEST(free_is_idempotent) {
    /* [free] */
    Str s = {};
    CHECK(str_from_cstr(&s, "temporary"));
    str_free(&s);
    str_free(&s);
    CHECK(str_from_cstr(&s, "reused"));
    /* [free] */
    CHECK_EQ_STR(str_cstr(&s), "reused");
    str_free(&s);
}

TEST(null_cstr_is_empty) {
    Str s = {};
    CHECK(str_from_cstr(&s, nullptr));
    CHECK(str_append_cstr(&s, nullptr));
    CHECK_EQ_INT(s.len, 0);
    CHECK_SV(sv_from_cstr(nullptr), "");
    str_free(&s);
}

TEST(with_capacity_preallocates) {
    Str s = {};
    REQUIRE(str_with_capacity(&s, 100));
    char *before = s.data;
    for (int i = 0; i < 100; i++) CHECK(str_push(&s, 'x'));
    CHECK(s.data == before);
    str_free(&s);

    CHECK(str_with_capacity(&s, 0));
    CHECK(s.data != nullptr);
    CHECK_EQ_STR(str_cstr(&s), "");
    str_free(&s);
}

/* --- Capacity ------------------------------------------------------------ */

TEST(grows_past_initial_capacity) {
    Str s = {};
    for (int i = 0; i < 1000; i++) CHECK(str_push(&s, 'a'));
    CHECK_EQ_INT(s.len, 1000);
    CHECK(s.data[1000] == '\0');
    CHECK(str_shrink_to_fit(&s));
    CHECK_EQ_INT(s.capacity, 1000);
    str_free(&s);
}

TEST(clear_keeps_capacity) {
    /* [clear] */
    Str s = {};
    CHECK(str_with_capacity(&s, 100));
    CHECK(str_append_cstr(&s, "data"));
    str_clear(&s);
    CHECK_EQ_STR(str_cstr(&s), "");
    CHECK(s.capacity >= 100);
    str_free(&s);
    /* [clear] */
}

TEST(reserve_rejects_overflow) {
    Str s = {};
    REQUIRE(str_from_cstr(&s, "abc"));
    Str before = s;
    CHECK(!str_reserve(&s, SIZE_MAX));     /* len + additional overflows */
    CHECK(!str_reserve(&s, SIZE_MAX - 3)); /* capacity + 1 overflows */
    CHECK(s.data == before.data &&
          s.len == before.len &&
          s.capacity == before.capacity);
    CHECK_EQ_STR(str_cstr(&s), "abc");
    str_free(&s);

    CHECK(!str_with_capacity(&s, SIZE_MAX));
    CHECK(s.data == nullptr);
}

/* --- Mutation ------------------------------------------------------------ */

TEST(appendf) {
    /* [appendf] */
    Str s = {};
    CHECK(str_appendf(&s, "%d-%s", 42, "x"));
    CHECK(str_appendf(&s, "|%05.1f", 3.14159));
    CHECK_EQ_STR(str_cstr(&s), "42-x|003.1");
    str_free(&s);
    /* [appendf] */
}

TEST(appendf_edges) {
    Str s = {};
    CHECK(str_appendf(&s, "%s", ""));
    CHECK_EQ_INT(s.len, 0);
    CHECK_EQ_STR(str_cstr(&s), "");

    CHECK(str_appendf(&s, "%0500d", 7));
    CHECK_EQ_INT(s.len, 500);
    CHECK(s.data[0] == '0' && s.data[499] == '7' && s.data[500] == '\0');
    str_free(&s);
}

TEST(embedded_nul_bytes) {
    Str s = {};
    CHECK(str_append(&s, (StrView){ .data = "a\0b", .len = 3 }));
    CHECK(str_push(&s, '\0'));
    CHECK_SV(str_view(&s), "a\0b\0");
    CHECK(!str_eq(str_view(&s), SV("a")));
    str_free(&s);
}

/* A temporary file holding a repeating byte pattern, NULs included. */
static FILE *pattern_file(size_t n) {
    FILE *f = tmpfile();
    if (f == nullptr) return nullptr;
    for (size_t i = 0; i < n; i++) fputc((int)(i * 7 % 256), f);
    rewind(f);
    return f;
}

TEST(append_file) {
    /* [append_file] */
    FILE *f = tmpfile();
    REQUIRE(f != nullptr);
    fputs("line one\nline two\n", f);
    rewind(f);

    Str s = {};
    CHECK(str_append_file(&s, f));
    CHECK_EQ_STR(str_cstr(&s), "line one\nline two\n");
    str_free(&s);
    fclose(f);
    /* [append_file] */
}

TEST(append_file_binary_onto_existing) {
    FILE *f = pattern_file(10000);
    REQUIRE(f != nullptr);
    Str s = {};
    REQUIRE(str_from_cstr(&s, "head:"));
    CHECK(str_append_file(&s, f));
    CHECK_EQ_INT(s.len, 5 + 10000);
    bool same = memcmp(s.data, "head:", 5) == 0;
    for (size_t i = 0; i < 10000; i++) {
        same = same && (size_t)(unsigned char)s.data[5 + i] == i * 7 % 256;
    }
    CHECK(same);
    CHECK(s.data[s.len] == '\0');
    str_free(&s);
    fclose(f);
}

TEST(append_file_empty) {
    FILE *f = tmpfile();
    REQUIRE(f != nullptr);
    Str s = {};
    CHECK(str_append_file(&s, f));
    CHECK_EQ_INT(s.len, 0);
    CHECK_EQ_STR(str_cstr(&s), "");

    REQUIRE(str_append_cstr(&s, "kept"));
    rewind(f);
    CHECK(str_append_file(&s, f));
    CHECK_EQ_STR(str_cstr(&s), "kept");
    str_free(&s);
    fclose(f);
}

TEST(self_append) {
    /* [self_append] */
    Str s = {};
    CHECK(str_from_cstr(&s, "abc"));
    CHECK(str_append(&s, str_view(&s)));
    CHECK_EQ_STR(str_cstr(&s), "abcabc");
    str_free(&s);
    /* [self_append] */
}

TEST(self_append_grows_repeatedly) {
    Str s = {};
    CHECK(str_from_cstr(&s, "abc"));
    for (int i = 0; i < 6; i++) CHECK(str_append(&s, str_view(&s)));
    CHECK_EQ_INT(s.len, 3 * 64);
    CHECK(str_eq((StrView){ s.data + 189, 3 }, SV("abc")));
    str_free(&s);
}

TEST(self_append_slice) {
    Str s = {};
    REQUIRE(str_from_cstr(&s, "0123456789"));
    REQUIRE(str_shrink_to_fit(&s)); /* force the append to reallocate */
    CHECK(str_append(&s, (StrView){ s.data + 3, 4 }));
    CHECK_EQ_STR(str_cstr(&s), "01234567893456");
    str_free(&s);
}

/* --- Access -------------------------------------------------------------- */

TEST(equality) {
    /* [eq] */
    CHECK(str_eq(SV("abc"), sv_from_cstr("abc")));
    CHECK(!str_eq(SV("abc"), SV("abd")));
    CHECK(!str_eq(SV("abc"), SV("ab")));
    /* [eq] */
    CHECK(str_eq(SV(""), sv_from_cstr(nullptr)));
    CHECK(str_eq((StrView){}, SV("")));
}

TEST(fmt_prints_exact_bytes) {
    /* [fmt] */
    Str s = {};
    CHECK(str_from_cstr(&s, "hello"));
    char buf[32];
    snprintf(buf, sizeof buf, "[%.*s]", STR_FMT(s));
    CHECK_EQ_STR(buf, "[hello]");
    str_free(&s);
    /* [fmt] */
}

/* --- Views --------------------------------------------------------------- */

TEST(find) {
    /* [find] */
    StrView v = SV("key=value=more");
    size_t at = sv_find(v, SV("="));
    CHECK_EQ_INT(at, 3);
    CHECK_SV(sv_slice(v, 0, at), "key");
    CHECK_SV(sv_slice(v, at + 1, v.len), "value=more");
    CHECK(sv_find(v, SV("missing")) == SV_NPOS);
    /* [find] */
}

TEST(find_edges) {
    CHECK_EQ_INT(sv_find(SV("aaab"), SV("aab")), 1); /* after a partial match */
    CHECK_EQ_INT(sv_find(SV("xyzabc"), SV("abc")), 3);
    CHECK_EQ_INT(sv_find(SV("abc"), SV("")), 0);
    CHECK_EQ_INT(sv_find((StrView){}, SV("")), 0);
    CHECK(sv_find((StrView){}, SV("a")) == SV_NPOS);
    CHECK(sv_find(SV("ab"), SV("abc")) == SV_NPOS);
    CHECK_EQ_INT(sv_find(SV("a\0b\0c"), SV("b\0c")), 2);

    CHECK_EQ_INT(sv_find_byte(SV("hello"), 'l'), 2);
    CHECK_EQ_INT(sv_find_byte(SV("a\0b"), '\0'), 1);
    CHECK(sv_find_byte(SV("hello"), 'z') == SV_NPOS);
    CHECK(sv_find_byte((StrView){}, 'a') == SV_NPOS);
}

TEST(compare) {
    CHECK(sv_cmp(SV("abc"), SV("abd")) < 0);
    CHECK(sv_cmp(SV("abd"), SV("abc")) > 0);
    CHECK(sv_cmp(SV("ab"), SV("abc")) < 0); /* a prefix sorts first */
    CHECK(sv_cmp(SV("abc"), SV("abc")) == 0);
    CHECK(sv_cmp((StrView){}, SV("")) == 0);
    CHECK(sv_cmp(SV("a\xff"), SV("a\x01")) > 0); /* bytes compare unsigned */
    CHECK(sv_cmp(SV("a\0"), SV("a")) > 0);       /* strcmp calls these equal */

    CHECK(sv_starts_with(SV("prefix.rest"), SV("prefix")));
    CHECK(sv_starts_with(SV("abc"), SV("")));
    CHECK(!sv_starts_with(SV("pre"), SV("prefix")));
    CHECK(sv_ends_with(SV("file.c"), SV(".c")));
    CHECK(!sv_ends_with(SV("file.h"), SV(".c")));
    CHECK(!sv_ends_with(SV("c"), SV("file.c")));
}

TEST(slice_and_trim) {
    StrView v = SV("hello");
    CHECK_SV(sv_slice(v, 1, 3), "el");
    CHECK_SV(sv_slice(v, 0, 0), "");
    CHECK_SV(sv_slice(v, 5, 5), "");
    CHECK_SV(sv_slice((StrView){}, 0, 0), "");

    CHECK_SV(sv_trim(SV("  \t a b \r\n")), "a b");
    CHECK_SV(sv_trim(SV(" \n ")), "");
    CHECK_SV(sv_trim(SV("x")), "x");
    CHECK_SV(sv_trim((StrView){}), "");
}

TEST(split) {
    /* [split] */
    StrView rest = SV("a,,b,"), piece;
    Str got = {};
    while (sv_split_next(&rest, SV(","), &piece)) {
        CHECK(str_appendf(&got, "[%.*s]", STR_FMT(piece)));
    }
    CHECK_EQ_STR(str_cstr(&got), "[a][][b]");
    str_free(&got);
    /* [split] */
}

/* Each piece in brackets, so no pieces ("") and one empty piece ("[]") differ.
 */
static void check_split(const char *in, const char *sep, const char *want) {
    Str got = {};
    StrView rest = sv_from_cstr(in), piece;
    while (sv_split_next(&rest, sv_from_cstr(sep), &piece)) {
        CHECK(str_appendf(&got, "[%.*s]", STR_FMT(piece)));
    }
    CHECK_EQ_STR(str_cstr(&got), want);
    str_free(&got);
}

TEST(split_edges) {
    check_split("", ",", "");
    check_split("a", ",", "[a]");
    check_split(",", ",", "[]");
    check_split(",a", ",", "[][a]");
    check_split("a,b", ",", "[a][b]");
    check_split("one\r\ntwo\r\n", "\r\n", "[one][two]");
    check_split("a\rb\r\n", "\r\n", "[a\rb]");
}

TEST(tokens) {
    /* [token] */
    StrView rest = SV("  the quick,, brown  "), word;
    Str got = {};
    while (sv_token_next(&rest, SV(" ,"), &word)) {
        CHECK(str_appendf(&got, "[%.*s]", STR_FMT(word)));
    }
    CHECK_EQ_STR(str_cstr(&got), "[the][quick][brown]");
    str_free(&got);
    /* [token] */
    CHECK_EQ_INT(rest.len, 0);

    StrView only_delims = SV(" , ");
    CHECK(!sv_token_next(&only_delims, SV(" ,"), &word));
    StrView no_delims = SV("abc");
    CHECK(sv_token_next(&no_delims, SV(""), &word));
    CHECK_SV(word, "abc");
}

/* --- Out of memory ------------------------------------------------------- */

static bool op_push(Str *s) {
    return str_push(s, 'x');
}
static bool op_append(Str *s) {
    return str_append(s, SV("a tail long enough to force growth"));
}
static bool op_append_self(Str *s) {
    return str_append(s, str_view(s));
}
static bool op_appendf(Str *s) {
    return str_appendf(s, "%d-%s", 12345, "formatted");
}
static bool op_reserve(Str *s) {
    return str_reserve(s, 100);
}
static bool op_shrink(Str *s) {
    return str_shrink_to_fit(s);
}
static FILE *op_file;
static bool op_append_file(Str *s) {
    rewind(op_file);
    return str_append_file(s, op_file);
}

/* Fail the first allocation, then the second, and so on until `op` succeeds.
 * Each failure must return false and leave `s` exactly as it was. */
static void check_unchanged_on_oom(Str *s, bool (*op)(Str *),
                                   const char *op_name) {
    for (long k = 0; k < 8; k++) {
        Str before = *s;
        char bytes[64] = {};
        if (s->data != nullptr) memcpy(bytes, s->data, s->len + 1);

        test_fail_alloc_after(k);
        bool ok = op(s);
        test_fail_alloc_after(-1);
        if (ok) return;

        bool same = s->data == before.data &&
                    s->len == before.len &&
                    s->capacity == before.capacity &&
                    (s->data == nullptr ||
                     memcmp(bytes, s->data, s->len + 1) == 0);
        if (!same)
            printf("  %s changed the string after allocation %ld failed\n",
                   op_name, k + 1);
        CHECK(same);
    }
    printf("  %s still failing after 8 allocations\n", op_name);
    CHECK(false);
}

TEST(oom_leaves_string_unchanged) {
    static const struct {
        bool (*fn)(Str *);
        const char *name;
    } ops[] = {
        { op_push, "push" },
        { op_append, "append" },
        { op_append_self, "append_self" },
        { op_appendf, "appendf" },
        { op_reserve, "reserve" },
        { op_shrink, "shrink" },
        { op_append_file, "append_file" },
    };
    op_file = pattern_file(10000); /* several reads, so several allocations */
    REQUIRE(op_file != nullptr);
    for (size_t i = 0; i < sizeof ops / sizeof ops[0]; i++) {
        Str empty = {};
        check_unchanged_on_oom(&empty, ops[i].fn, ops[i].name);
        str_free(&empty);

        /* Exactly full, so the next growth must reallocate. */
        Str full = {};
        REQUIRE(str_from_cstr(&full, "abc"));
        REQUIRE(str_shrink_to_fit(&full));
        check_unchanged_on_oom(&full, ops[i].fn, ops[i].name);
        str_free(&full);

        /* Some spare room, but less than any op appends. */
        Str partial = {};
        REQUIRE(str_from_cstr(&partial, "abc"));
        REQUIRE(partial.capacity - partial.len < 15);
        check_unchanged_on_oom(&partial, ops[i].fn, ops[i].name);
        str_free(&partial);
    }
    fclose(op_file);
}

TEST(oom_during_init_leaves_empty) {
    Str s = {};
    test_fail_alloc_after(0);
    CHECK(!str_from_cstr(&s, "abc"));
    CHECK(!str_with_capacity(&s, 10));
    test_fail_alloc_after(-1);
    CHECK(s.data == nullptr && s.len == 0 && s.capacity == 0);
}

/* --- Model --------------------------------------------------------------- */

/* splitmix64: tiny and identical on every platform, unlike rand(). */
static uint64_t next_random(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15u);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9u;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBu;
    return z ^ (z >> 31);
}

/* Random operations on a Str, checked after every step against a plain array
 * that applies the same operations. */
TEST(matches_model_under_random_ops) {
    static const char pool[] = "the quick brown fox\0jumps over\tthe lazy dog";
    char model[512];
    size_t model_len = 0;
    Str s = {};
    uint64_t rng = 1;

    for (int step = 0; step < 20000; step++) {
        if (model_len > 200) {
            str_clear(&s);
            model_len = 0;
        }
        uint64_t r = next_random(&rng);
        int op = (int)(r % 6);
        r /= 6;
        bool ok = true;

        switch (op) {
        case 0: {
            char c = (char)(r & 0xff);
            ok = str_push(&s, c);
            model[model_len++] = c;
            break;
        }
        case 1: {
            size_t off = (size_t)(r % sizeof pool);
            size_t n = (size_t)((r >> 16) % (sizeof pool - off));
            ok = str_append(&s, (StrView){ pool + off, n });
            memcpy(model + model_len, pool + off, n);
            model_len += n;
            break;
        }
        case 2: {
            size_t off = (size_t)(r % (model_len + 1));
            size_t n = (size_t)((r >> 16) % (model_len - off + 1));
            ok = str_append(&s, (StrView){ str_cstr(&s) + off, n });
            memmove(model + model_len, model + off, n);
            model_len += n;
            break;
        }
        case 3: {
            unsigned v = (unsigned)(r % 100000);
            ok = str_appendf(&s, "%u", v);
            model_len += (size_t)snprintf(model + model_len,
                                          sizeof model - model_len, "%u", v);
            break;
        }
        case 4:
            str_clear(&s);
            model_len = 0;
            break;
        default:
            ok = str_shrink_to_fit(&s);
            break;
        }

        bool same = ok &&
                    s.len == model_len &&
                    s.capacity >= s.len &&
                    memcmp(str_cstr(&s), model, model_len) == 0 &&
                    str_cstr(&s)[s.len] == '\0';
        if (!same) printf("  diverged at step %d (op %d)\n", step, op);
        REQUIRE(same);
    }
    str_free(&s);
}

/* Up to `max` random bytes from `alphabet`, NUL-terminated for string.h. */
static size_t random_text(uint64_t *rng, char *buf, size_t max,
                          const char *alphabet) {
    size_t n = (size_t)(next_random(rng) % (max + 1));
    size_t k = strlen(alphabet);
    for (size_t i = 0; i < n; i++) buf[i] = alphabet[next_random(rng) % k];
    buf[n] = '\0';
    return n;
}

/* sv_find, sv_find_byte and sv_cmp against strstr, strchr and strcmp. A
 * two-letter alphabet makes partial matches common. */
TEST(views_match_string_h) {
    uint64_t rng = 2;
    char hay[41], needle[6], other[41];
    for (int step = 0; step < 20000; step++) {
        StrView h = { hay, random_text(&rng, hay, 40, "ab") };
        StrView n = { needle, random_text(&rng, needle, 5, "ab") };
        StrView o = { other, random_text(&rng, other, 40, "ab") };

        const char *p = strstr(hay, needle);
        const char *q = strchr(hay, 'b');
        int want = strcmp(hay, other), got = sv_cmp(h, o);
        bool same = sv_find(h, n) == (p ? (size_t)(p - hay) : SV_NPOS) &&
                    sv_find_byte(h, 'b') == (q ? (size_t)(q - hay) : SV_NPOS) &&
                    (want < 0) == (got < 0) &&
                    (want > 0) == (got > 0);
        if (!same) {
            printf("  diverged at step %d: \"%s\" \"%s\" \"%s\"\n", step, hay,
                   needle, other);
        }
        REQUIRE(same);
    }
}

/* sv_token_next against strtok on text with runs of mixed delimiters. */
TEST(tokens_match_strtok) {
    uint64_t rng = 3;
    char text[61], copy[61];
    for (int step = 0; step < 20000; step++) {
        size_t n = random_text(&rng, text, 60, "ab ,;");
        memcpy(copy, text, n + 1);
        StrView rest = { text, n }, tok;

        bool same = true;
        for (char *t = strtok(copy, " ,;"); t; t = strtok(nullptr, " ,;")) {
            same = same &&
                   sv_token_next(&rest, SV(" ,;"), &tok) &&
                   str_eq(tok, sv_from_cstr(t));
        }
        same = same && !sv_token_next(&rest, SV(" ,;"), &tok);
        if (!same) printf("  diverged at step %d: \"%s\"\n", step, text);
        REQUIRE(same);
    }
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(zero_init_is_empty);
    RUN(from_and_append);
    RUN(free_is_idempotent);
    RUN(null_cstr_is_empty);
    RUN(with_capacity_preallocates);
    RUN(grows_past_initial_capacity);
    RUN(clear_keeps_capacity);
    RUN(reserve_rejects_overflow);
    RUN(appendf);
    RUN(appendf_edges);
    RUN(embedded_nul_bytes);
    RUN(append_file);
    RUN(append_file_binary_onto_existing);
    RUN(append_file_empty);
    RUN(self_append);
    RUN(self_append_grows_repeatedly);
    RUN(self_append_slice);
    RUN(equality);
    RUN(fmt_prints_exact_bytes);
    RUN(find);
    RUN(find_edges);
    RUN(compare);
    RUN(slice_and_trim);
    RUN(split);
    RUN(split_edges);
    RUN(tokens);
    RUN(oom_leaves_string_unchanged);
    RUN(oom_during_init_leaves_empty);
    RUN(matches_model_under_random_ops);
    RUN(views_match_string_h);
    RUN(tokens_match_strtok);
    return TEST_REPORT();
}
