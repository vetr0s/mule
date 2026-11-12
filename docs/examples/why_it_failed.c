/* err: report why a call failed, and branch on the kind, not the text. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

/* A port in 1..65535, from text a user typed. */
static bool parse_port(StrView text, uint16_t *port, Err *err) {
    uint64_t n;
    if (!sv_parse_u64(sv_trim(text), 10, &n) || n == 0 || n > 65535) {
        err_set(err, ERR_INVALID, 0, "port: expected 1 to 65535, got '%.*s'",
                STR_FMT(text));
        return false;
    }
    *port = (uint16_t)n;
    return true;
}

int main(void) {
    const char *inputs[] = { "8080", "70000", " 443 " };
    for (size_t i = 0; i < 3; i++) {
        Err err = { 0 };
        uint16_t port;
        if (parse_port(sv_from_cstr(inputs[i]), &port, &err))
            printf("listening on %u\n", port);
        else
            printf("%s\n", err.msg);
    }

    Err err = { 0 };
    Str text = { 0 };
    if (!fs_read(&text, "no/such/config.ini", &err) &&
        err.kind == ERR_NOT_FOUND)
        printf("no config, using defaults (%s)\n", err.msg);
    str_free(&text);
    return EXIT_SUCCESS;
}
