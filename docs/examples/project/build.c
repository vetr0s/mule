/* The build for a one-file program, written in C against MULE.
 *
 *   cc build.c && ./a.out        build .build/hello
 *   ./a.out run                  build it, then run it
 *
 * A project copies mule.h beside build.c and includes "mule.h". This one
 * lives in MULE's repository, so it reaches into dist/ instead.
 */
#define MULE_IMPLEMENTATION
#include "../../../dist/mule.h"

#include <string.h>

int main(int argc, char **argv) {
    /* [build] */
    MULE_REBUILD_SELF(argc, argv);

    Err err = { 0 };
    Cmd cc = { 0 };
    bool needed = false;
    bool ok = cmd_append(&cc, "cc", "-std=c11", "-Wall", "-Wextra", "-MMD",
                         "-MF", ".build/hello.d", "-o", ".build/hello",
                         "hello.c") &&
              fs_mkdir_p(".build", &err) &&
              build_prepare(".build/hello", ".build/hello.d", &cc, &needed,
                            &err);
    if (ok && needed) ok = build_run(&cc, &err);
    /* [build] */

    if (ok && argc > 1 && strcmp(argv[1], "run") == 0) {
        arr_clear(&cc);
        ok = cmd_append(&cc, ".build/hello") && build_run(&cc, &err);
    }
    if (!ok) log_error("%s", err.msg);
    arr_free(&cc);
    return ok ? 0 : 1;
}
