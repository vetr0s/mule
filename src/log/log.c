#include "log/log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Set to anything other than empty or "0". */
static bool log_env_on_(const char *name) {
    const char *v = getenv(name);
    return v != nullptr && v[0] != '\0' && strcmp(v, "0") != 0;
}

bool log_color_enabled(FILE *f) {
    const char *no_color = getenv("NO_COLOR");
    if (no_color != nullptr && no_color[0] != '\0') return false;
    if (log_env_on_("CLICOLOR_FORCE") || log_env_on_("FORCE_COLOR"))
        return true;
    if (!isatty(fileno(f))) return false;

    const char *term = getenv("TERM");
    return term != nullptr && term[0] != '\0' && strcmp(term, "dumb") != 0;
}
