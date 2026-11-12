/* A scratch directory for tests that touch the disk. Include it after
 * fs/fs.c. Each test calls fresh() first, and main calls scratch_done() last.
 */
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static char root_[256];

/* Empty the scratch directory, $TMPDIR/mule-test-<pid>, and return it. */
MULE_UNUSED static const char *fresh(void) {
    const char *tmp = getenv("TMPDIR");
    snprintf(root_, sizeof root_, "%s/mule-test-%ld",
             tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp", (long)getpid());
    Err err = { 0 };
    if (!fs_remove_all(root_, &err) || !fs_mkdir_p(root_, &err)) {
        printf("  scratch: %s\n", err.msg);
        exit(EXIT_FAILURE);
    }
    return root_;
}

/* `name` under the scratch directory, in `buf`. */
MULE_UNUSED static const char *at(char buf[512], const char *name) {
    snprintf(buf, 512, "%s/%s", root_, name);
    return buf;
}

/* Write `text` to `name` under the scratch directory. */
MULE_UNUSED static bool put(const char *name, const char *text) {
    char p[512];
    return fs_write(at(p, name), sv_from_cstr(text), NULL);
}

MULE_UNUSED static void scratch_done(void) {
    if (root_[0] != '\0' && !fs_remove_all(root_, NULL))
        printf("  could not remove %s\n", root_);
}
