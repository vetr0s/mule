#pragma once
/*
 * Str: an owned, growable, heap-allocated byte string.
 *
 *   - `data` is always NUL-terminated when non-null, so it can be passed to
 *     C APIs, but `len` is authoritative (embedded NULs are allowed).
 *   - `capacity` is the number of bytes usable for content, excluding the
 *     terminator (the allocation is `capacity + 1` bytes).
 *   - A zero-initialised Str (`Str s = {};`) is a valid empty string and owns
 *     no memory; every function accepts it.
 *   - Functions that may allocate return `bool` (false = out of memory or
 *     size overflow). On failure the string is left unchanged.
 */

#include <stddef.h>

typedef struct Str {
    char  *data;
    size_t len;
    size_t capacity;
} Str;

/* Non-owning view of bytes; handy for passing slices and literals around. */
typedef struct StrView {
    const char *data;
    size_t      len;
} StrView;

#define SV(lit) ((StrView){ .data = (lit), .len = sizeof(lit) - 1 })
/* printf helpers: printf("%.*s", STR_FMT(s)) */
#define STR_FMT(s) (int)(s).len, (s).data

/* --- Lifetime ------------------------------------------------------------ */
[[nodiscard]] bool str_with_capacity(Str *s, size_t capacity);
[[nodiscard]] bool str_from(Str *s, StrView v);
[[nodiscard]] bool str_from_cstr(Str *s, const char *cstr);
void str_free(Str *s);

/* --- Capacity ------------------------------------------------------------ */
[[nodiscard]] bool str_reserve(Str *s, size_t additional);
[[nodiscard]] bool str_shrink_to_fit(Str *s);
void str_clear(Str *s);

/* --- Mutation ------------------------------------------------------------ */
[[nodiscard]] bool str_push(Str *s, char c);
[[nodiscard]] bool str_append(Str *s, StrView v);
[[nodiscard]] bool str_append_cstr(Str *s, const char *cstr);
[[nodiscard]] bool str_appendf(Str *s, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/* --- Access -------------------------------------------------------------- */
[[nodiscard]] StrView str_view(const Str *s);
[[nodiscard]] const char *str_cstr(const Str *s);  /* never null */
[[nodiscard]] bool str_eq(StrView a, StrView b);
[[nodiscard]] StrView sv_from_cstr(const char *cstr);
