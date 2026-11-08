#include "log/log.h"
#include "str/str.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

LogLevel log_level_ = LOG_INFO;
static FILE *log_out_;      /* null means stderr */
static int log_color_ = -1; /* -1 until the first message decides */

static const char *const log_names_[] = {
    [LOG_TRACE] = "TRACE", [LOG_DEBUG] = "DEBUG", [LOG_INFO] = "INFO ",
    [LOG_WARN] = "WARN ",  [LOG_ERROR] = "ERROR", [LOG_FATAL] = "FATAL",
};
static const char *const log_colors_[] = {
    [LOG_TRACE] = "",        [LOG_DEBUG] = "",      [LOG_INFO] = LOG_GREEN,
    [LOG_WARN] = LOG_YELLOW, [LOG_ERROR] = LOG_RED, [LOG_FATAL] = LOG_RED,
};

void log_set_level(LogLevel level) {
    log_level_ = level;
}

void log_set_output(FILE *f) {
    log_out_ = f;
    log_color_ = -1;
}

/* Set to anything other than empty or "0". */
static bool log_env_on_(const char *name) {
    const char *v = getenv(name);
    return v != NULL && v[0] != '\0' && strcmp(v, "0") != 0;
}

bool log_color_enabled(FILE *f) {
    const char *no_color = getenv("NO_COLOR");
    if (no_color != NULL && no_color[0] != '\0') return false;
    if (log_env_on_("CLICOLOR_FORCE") || log_env_on_("FORCE_COLOR"))
        return true;
    if (!isatty(fileno(f))) return false;

    const char *term = getenv("TERM");
    return term != NULL && term[0] != '\0' && strcmp(term, "dumb") != 0;
}

MULE_PRINTF(4, 0)
static FILE *log_vwrite_(LogLevel level, const char *file, int line,
                         const char *fmt, va_list ap) {
    FILE *out = log_out_ != NULL ? log_out_ : stderr;
    if (log_color_ < 0) log_color_ = log_color_enabled(out);
    const char *on = log_color_ ? log_colors_[level] : "";
    const char *off = on[0] != '\0' ? LOG_RESET : "";

    /* shortcut: one allocation per line. Reuse a thread_local buffer if
     * logging ever shows up in a profile. */
    Str s = { 0 };
    bool ok = str_appendf(&s, "%s%s%s %s:%d: ", on, log_names_[level], off,
                          file, line) &&
              str_vappendf(&s, fmt, ap) &&
              str_push(&s, '\n');
    if (ok)
        fwrite(s.data, 1, s.len, out);
    else
        fprintf(out, "%s %s:%d: (out of memory formatting this message)\n",
                log_names_[level], file, line);
    str_free(&s);
    return out;
}

void log_write_(LogLevel level, const char *file, int line, const char *fmt,
                ...) {
    va_list ap;
    va_start(ap, fmt);
    log_vwrite_(level, file, line, fmt, ap);
    va_end(ap);
}

void log_fatal_(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    FILE *out = log_vwrite_(LOG_FATAL, file, line, fmt, ap);
    va_end(ap);
    fflush(out);
    abort();
}
