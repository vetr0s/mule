/* The second file of the consumer build. It uses each module's macros and
 * inline helpers, which are compiled again in every file that includes the
 * header.
 */
#include "mule.h"

typedef Arr(int) IntArr;

int consumer_header_only(void);

int consumer_header_only(void) {
    int failed = 0;

    IntArr xs = { 0 };
    failed |= !(arr_push(&xs, 2) && arr_insert(&xs, 0, 1) && xs.len == 2);
    arr_free(&xs);

    Str s = { 0 };
    failed |= !(str_appendf(&s, "%s %d", MULE_VERSION, 3) &&
                sv_ends_with(str_view(&s), SV(" 3")));
    str_free(&s);

    size_t sum = 0;
    failed |= ckd_add(&sum, (size_t)1, (size_t)2) || sum != 3;

    /* Only a log call reads it, which LOG_LEVEL_MIN compiles out. */
    int detail = 42;
    log_debug("detail %d", detail);

    Err err = { 0 };
    err_set(&err, ERR_INVALID, 0, "consumer");
    failed |= err.kind != ERR_INVALID;
    return failed;
}
