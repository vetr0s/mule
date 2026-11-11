#include "err/err.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void err_set(Err *e, ErrKind kind, int code, const char *fmt, ...) {
    if (e == NULL) return;
    e->kind = kind;
    e->code = code;
    va_list ap;
    va_start(ap, fmt);
    if (vsnprintf(e->msg, sizeof e->msg, fmt, ap) < 0) e->msg[0] = '\0';
    va_end(ap);
}

static ErrKind err_kind_(int code) {
    switch (code) {
    case ENOMEM:
        return ERR_NO_MEMORY;
    case ENOENT:
        return ERR_NOT_FOUND;
    case EEXIST:
        return ERR_EXISTS;
    case EACCES:
    case EPERM:
        return ERR_PERMISSION;
    case ENOTDIR:
        return ERR_NOT_DIR;
    case EISDIR:
        return ERR_IS_DIR;
    default:
        return ERR_OS;
    }
}

void err_os(Err *e, int code, const char *op, StrView subject) {
    if (e == NULL) return;
    char buf[128];
    /* A program that defines _GNU_SOURCE gets glibc's strerror_r, which
     * returns the text instead of filling buf. */
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
    const char *reason = strerror_r(code, buf, sizeof buf);
#else
    const char *reason = strerror_r(code, buf, sizeof buf) == 0 ? buf : "";
#endif
    if (subject.len == 0)
        err_set(e, err_kind_(code), code, "%s: %s", op, reason);
    else
        err_set(e, err_kind_(code), code, "%s %.*s: %s", op, STR_FMT(subject),
                reason);
}
