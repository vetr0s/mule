/* proc: capture a command's output, report a failure, and run jobs at once. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    Err err = { 0 };
    Cmd c = { 0 };
    Str out = { 0 };
    bool ok = cmd_append(&c, "uname", "-s") && cmd_capture(&c, &out, &err);
    printf("uname -s: %s", ok ? str_cstr(&out) : err.msg);

    /* A command that fails says how. */
    arr_clear(&c);
    if (cmd_append(&c, "sh", "-c", "exit 3") && !cmd_run(&c, &err))
        printf("%s (kind %d, code %d)\n", err.msg, err.kind, err.code);

    /* Three jobs, at most two at a time. */
    ProcPool pool = { .max = 2 };
    const char *words[] = { "one", "two", "three" };
    bool ran = true;
    for (size_t i = 0; ran && i < 3; i++) {
        arr_clear(&c);
        ran = cmd_append(&c, "echo", words[i]) &&
              proc_pool_start(&pool, &c, &err);
    }
    ran = proc_pool_wait(&pool, &err) && ran;
    if (!ran) printf("%s\n", err.msg);

    proc_pool_free(&pool);
    str_free(&out);
    arr_free(&c);
    return ok && ran ? EXIT_SUCCESS : EXIT_FAILURE;
}
