/* fs: walk a source tree in sorted order and count the lines of each file.
 *
 *   line_count [dir]    counts .c and .h files under dir, or src
 */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct Count {
    size_t files, lines;
    Err *err;
} Count;

static FsWalkAct visit(void *ctx, const char *path, FsType type) {
    Count *c = ctx;
    StrView ext = fs_extension(sv_from_cstr(path));
    if (type != FS_FILE || !(str_eq(ext, SV(".c")) || str_eq(ext, SV(".h"))))
        return FS_WALK_NEXT;

    Str text = { 0 };
    if (!fs_read(&text, path, c->err)) return FS_WALK_STOP;
    size_t lines = 0;
    for (size_t i = 0; i < text.len; i++) lines += text.data[i] == '\n';
    printf("%6zu  %.*s\n", lines, STR_FMT(fs_basename(sv_from_cstr(path))));
    c->files++;
    c->lines += lines;
    str_free(&text);
    return FS_WALK_NEXT;
}

int main(int argc, char **argv) {
    Err err = { 0 };
    Count c = { .err = &err };
    if (!fs_walk(argc > 1 ? argv[1] : "src", visit, &c, &err) ||
        err.kind != ERR_NONE) {
        fprintf(stderr, "line_count: %s\n", err.msg);
        return EXIT_FAILURE;
    }
    printf("%6zu  lines in %zu files\n", c.lines, c.files);
    return EXIT_SUCCESS;
}
