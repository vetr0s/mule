#include "err/err.h"

#include <stdlib.h>

void err_set(Err *e, ErrKind kind, int code, const char *fmt, ...) {
    (void)e, (void)kind, (void)code, (void)fmt;
    abort(); /* not implemented */
}

void err_os(Err *e, int code, const char *op, StrView subject) {
    (void)e, (void)code, (void)op, (void)subject;
    abort(); /* not implemented */
}
