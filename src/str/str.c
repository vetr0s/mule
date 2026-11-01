#include "str/str.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Grow so that `needed` content bytes fit. Amortised doubling. */
static bool str_grow_(Str *s, size_t needed) {
    if (needed <= s->capacity && s->data != nullptr) return true;

    size_t cap = s->capacity ? s->capacity : 15;
    while (cap < needed) {
        if (ckd_mul(&cap, cap, 2)) {
            cap = needed;
            break;
        }
    }

    size_t bytes;
    if (ckd_add(&bytes, cap, 1)) return false;

    char *p = realloc(s->data, bytes);
    if (p == nullptr) return false;
    if (s->data == nullptr) p[0] = '\0';

    s->data = p;
    s->capacity = cap;
    return true;
}

bool str_with_capacity(Str *s, size_t capacity) {
    *s = (Str){};
    return str_grow_(s, capacity);
}

bool str_from(Str *s, StrView v) {
    *s = (Str){};
    return str_append(s, v);
}

bool str_from_cstr(Str *s, const char *cstr) {
    return str_from(s, sv_from_cstr(cstr));
}

void str_free(Str *s) {
    free(s->data);
    *s = (Str){};
}

bool str_reserve(Str *s, size_t additional) {
    size_t needed;
    if (ckd_add(&needed, s->len, additional)) return false;
    return str_grow_(s, needed);
}

bool str_shrink_to_fit(Str *s) {
    if (s->data == nullptr || s->capacity == s->len) return true;
    char *p = realloc(s->data, s->len + 1);
    if (p == nullptr) return false;
    s->data = p;
    s->capacity = s->len;
    return true;
}

void str_clear(Str *s) {
    s->len = 0;
    if (s->data != nullptr) s->data[0] = '\0';
}

bool str_push(Str *s, char c) {
    /* Locals, because a char store may alias `*s` and force reloads. */
    char *data = s->data;
    size_t len = s->len;
    if (data != nullptr && len < s->capacity) {
        data[len] = c;
        data[len + 1] = '\0';
        s->len = len + 1;
        return true;
    }
    return str_append(s, (StrView){ .data = &c, .len = 1 });
}

bool str_append(Str *s, StrView v) {
    if (v.len == 0) return true;

    /* Fast path: it fits, so the buffer cannot move under `v`. */
    if (s->data != nullptr && v.len <= s->capacity - s->len) {
        memmove(s->data + s->len, v.data, v.len);
        s->len += v.len;
        s->data[s->len] = '\0';
        return true;
    }

    /* `v` may point into our own buffer (e.g. str_append(&s, str_view(&s))).
     * Remember its offset, since growing may move the allocation. */
    const bool aliased = s->data != nullptr &&
                         v.data >= s->data &&
                         v.data <= s->data + s->len;
    const size_t off = aliased ? (size_t)(v.data - s->data) : 0;

    if (!str_reserve(s, v.len)) return false;

    const char *src = aliased ? s->data + off : v.data;
    memmove(s->data + s->len, src, v.len);
    s->len += v.len;
    s->data[s->len] = '\0';
    return true;
}

bool str_append_cstr(Str *s, const char *cstr) {
    return str_append(s, sv_from_cstr(cstr));
}

bool str_appendf(Str *s, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);

    /* Format straight into the spare capacity; most calls fit and stop here. */
    size_t room = s->data != nullptr ? s->capacity - s->len + 1 : 0;
    char *end = room != 0 ? s->data + s->len : nullptr;
    int n = vsnprintf(end, room, fmt, ap);
    bool ok = n >= 0 && (size_t)n < room;
    if (ok) {
        s->len += (size_t)n;
    } else {
        /* The attempt overwrote the terminator; restore it before failing. */
        if (end != nullptr) *end = '\0';
        if (n >= 0 && str_reserve(s, (size_t)n)) {
            vsnprintf(s->data + s->len, (size_t)n + 1, fmt, ap2);
            s->len += (size_t)n;
            ok = true;
        }
    }

    va_end(ap2);
    va_end(ap);
    return ok;
}

bool str_append_file(Str *s, FILE *f) {
    /* Read into a scratch Str so a failure part-way leaves `s` untouched. */
    Str tmp = {};
    bool ok = true;
    for (;;) {
        if (!str_reserve(&tmp, 4096)) {
            ok = false;
            break;
        }
        size_t room = tmp.capacity - tmp.len;
        size_t n = fread(tmp.data + tmp.len, 1, room, f);
        tmp.len += n;
        if (n < room) {
            ok = !ferror(f);
            break;
        }
    }

    if (ok && s->data == nullptr) {
        /* `s` owns nothing, so take the buffer instead of copying it. */
        tmp.data[tmp.len] = '\0';
        *s = tmp;
        return true;
    }
    ok = ok && str_append(s, str_view(&tmp));
    str_free(&tmp);
    return ok;
}

StrView str_view(const Str *s) {
    return (StrView){ .data = str_cstr(s), .len = s->len };
}

const char *str_cstr(const Str *s) {
    return s->data != nullptr ? s->data : "";
}

bool str_eq(StrView a, StrView b) {
    return a.len == b.len && (a.len == 0 || memcmp(a.data, b.data, a.len) == 0);
}

StrView sv_from_cstr(const char *cstr) {
    if (cstr == nullptr) return (StrView){ .data = "", .len = 0 };
    return (StrView){ .data = cstr, .len = strlen(cstr) };
}

/* --- Views --------------------------------------------------------------- */

size_t sv_find_byte(StrView v, char c) {
    if (v.len == 0) return SV_NPOS;
    const char *p = memchr(v.data, c, v.len);
    return p != nullptr ? (size_t)(p - v.data) : SV_NPOS;
}

size_t sv_find(StrView v, StrView needle) {
    if (needle.len == 0) return 0;
    if (needle.len > v.len) return SV_NPOS;

    /* memchr to each candidate first byte, then compare the rest. */
    size_t last = v.len - needle.len;
    for (size_t i = 0; i <= last; i++) {
        const char *p = memchr(v.data + i, needle.data[0], last - i + 1);
        if (p == nullptr) return SV_NPOS;
        i = (size_t)(p - v.data);
        if (memcmp(p + 1, needle.data + 1, needle.len - 1) == 0) return i;
    }
    return SV_NPOS;
}

int sv_cmp(StrView a, StrView b) {
    size_t n = a.len < b.len ? a.len : b.len;
    int c = n != 0 ? memcmp(a.data, b.data, n) : 0;
    if (c != 0) return c;
    return (a.len > b.len) - (a.len < b.len);
}

bool sv_starts_with(StrView v, StrView prefix) {
    return prefix.len <= v.len && str_eq(sv_slice(v, 0, prefix.len), prefix);
}

bool sv_ends_with(StrView v, StrView suffix) {
    return suffix.len <= v.len &&
           str_eq(sv_slice(v, v.len - suffix.len, v.len), suffix);
}

StrView sv_slice(StrView v, size_t from, size_t to) {
    assert(from <= to && to <= v.len);
    /* An empty view may hold a null pointer, and null + 0 is undefined. */
    return (StrView){ .data = v.len != 0 ? v.data + from : v.data,
                      .len = to - from };
}

static bool sv_space_(char c) {
    return c == ' ' || (c >= '\t' && c <= '\r');
}

StrView sv_trim(StrView v) {
    size_t from = 0, to = v.len;
    while (from < to && sv_space_(v.data[from])) from++;
    while (to > from && sv_space_(v.data[to - 1])) to--;
    return sv_slice(v, from, to);
}

bool sv_split_next(StrView *rest, StrView sep, StrView *piece) {
    assert(sep.len != 0);
    if (rest->len == 0) return false;

    size_t at = sv_find(*rest, sep);
    if (at == SV_NPOS) {
        *piece = *rest;
        *rest = sv_slice(*rest, rest->len, rest->len);
    } else {
        *piece = sv_slice(*rest, 0, at);
        *rest = sv_slice(*rest, at + sep.len, rest->len);
    }
    return true;
}

static bool sv_in_set_(const uint64_t set[4], char c) {
    unsigned char u = (unsigned char)c;
    return (set[u >> 6] >> (u & 63)) & 1;
}

bool sv_token_next(StrView *rest, StrView delims, StrView *token) {
    /* One bit per byte value, like strspn builds, so each test is O(1). */
    uint64_t set[4] = {};
    for (size_t i = 0; i < delims.len; i++) {
        unsigned char c = (unsigned char)delims.data[i];
        set[c >> 6] |= UINT64_C(1) << (c & 63);
    }

    size_t from = 0, n = rest->len;
    while (from < n && sv_in_set_(set, rest->data[from])) from++;
    size_t to = from;
    while (to < n && !sv_in_set_(set, rest->data[to])) to++;

    if (from == to) {
        *rest = sv_slice(*rest, n, n);
        return false;
    }
    *token = sv_slice(*rest, from, to);
    *rest = sv_slice(*rest, to, n);
    return true;
}
