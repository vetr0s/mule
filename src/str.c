#include "str.h"

#include <stdarg.h>
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Grow so that `needed` content bytes fit. Amortised doubling. */
static bool str_grow_(Str *s, size_t needed) {
    if (needed <= s->capacity && s->data != nullptr) return true;

    size_t cap = s->capacity ? s->capacity : 15;
    while (cap < needed) {
        if (ckd_mul(&cap, cap, 2)) { cap = needed; break; }
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
    return str_append(s, (StrView){ .data = &c, .len = 1 });
}

bool str_append(Str *s, StrView v) {
    if (v.len == 0) return true;

    /* `v` may point into our own buffer (e.g. str_append(&s, str_view(&s))).
     * Remember its offset, since growing may move the allocation. */
    const bool aliased = s->data != nullptr && v.data >= s->data &&
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

    bool ok = false;
    int n = vsnprintf(nullptr, 0, fmt, ap);
    if (n >= 0 && str_reserve(s, (size_t)n)) {
        vsnprintf(s->data + s->len, (size_t)n + 1, fmt, ap2);
        s->len += (size_t)n;
        ok = true;
    }

    va_end(ap2);
    va_end(ap);
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
