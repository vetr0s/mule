/* Tests for src/err/err.c.
 *
 * Bracketed names in comment pairs mark regions that src/err/err.h pulls
 * into the Doxygen reference with @snippet. See tests/test_str.c for the rules.
 */
#include "test/test.h"

#include "err/err.c"
#include "log/log.c"
#include "mem/mem.c"
#include "str/str.c"

#include <errno.h>
#include <string.h>

/* [load_config] */
static bool load_config(Str *out, const char *path, Err *err) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        err_os(err, errno, "open", sv_from_cstr(path));
        return false;
    }
    bool ok = str_append_file(out, f);
    if (!ok && ferror(f))
        err_os(err, errno, "read", sv_from_cstr(path));
    else if (!ok)
        err_set(err, ERR_NO_MEMORY, 0, "read %s: out of memory", path);
    fclose(f);
    return ok;
}
/* [load_config] */

TEST(zero_err_is_none) {
    Err e = { 0 };
    CHECK(e.kind == ERR_NONE);
    CHECK_EQ_INT(e.code, 0);
    CHECK_EQ_STR(e.msg, "");
}

TEST(set_fills_every_part) {
    Err e = { 0 };
    err_set(&e, ERR_INVALID, 7, "--jobs: expected a number, got '%s'", "x");
    CHECK(e.kind == ERR_INVALID);
    CHECK_EQ_INT(e.code, 7);
    CHECK_EQ_STR(e.msg, "--jobs: expected a number, got 'x'");
}

TEST(set_cuts_a_long_message) {
    char long_path[ERR_MSG_MAX * 2];
    memset(long_path, 'a', sizeof long_path - 1);
    long_path[sizeof long_path - 1] = '\0';

    Err e = { 0 };
    err_set(&e, ERR_NOT_FOUND, 0, "open %s", long_path);
    CHECK_EQ_INT(strlen(e.msg), ERR_MSG_MAX - 1);
    CHECK(strncmp(e.msg, "open aaa", 8) == 0);
}

TEST(null_err_is_ignored) {
    err_set(NULL, ERR_INVALID, 1, "nobody reads this");
    err_os(NULL, ENOENT, "open", SV("x"));
}

TEST(os_maps_codes_to_kinds) {
    static const struct {
        int code;
        ErrKind kind;
    } cases[] = {
        { ENOMEM, ERR_NO_MEMORY }, { ENOENT, ERR_NOT_FOUND },
        { EEXIST, ERR_EXISTS },    { EACCES, ERR_PERMISSION },
        { EPERM, ERR_PERMISSION }, { ENOTDIR, ERR_NOT_DIR },
        { EISDIR, ERR_IS_DIR },    { EIO, ERR_OS },
        { ENOTEMPTY, ERR_OS },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        Err e = { 0 };
        err_os(&e, cases[i].code, "stat", SV("x"));
        CHECK_EQ_INT(e.kind, cases[i].kind);
        CHECK_EQ_INT(e.code, cases[i].code);
    }
}

TEST(os_writes_op_subject_and_reason) {
    char want[ERR_MSG_MAX];
    snprintf(want, sizeof want, "open build/main.o: %s", strerror(ENOENT));
    Err e = { 0 };
    err_os(&e, ENOENT, "open", SV("build/main.o"));
    CHECK_EQ_STR(e.msg, want);

    snprintf(want, sizeof want, "fork: %s", strerror(EAGAIN));
    err_os(&e, EAGAIN, "fork", SV(""));
    CHECK_EQ_STR(e.msg, want);
}

TEST(load_config_reports_why) {
    Str cfg = { 0 };
    /* [report] */
    Err err = { 0 };
    if (!load_config(&cfg, "no/such/mule.ini", &err) &&
        err.kind != ERR_NOT_FOUND)
        log_error("%s", err.msg);
    /* [report] */
    CHECK(err.kind == ERR_NOT_FOUND);
    CHECK(strncmp(err.msg, "open no/such/mule.ini: ", 23) == 0);

    Err untouched = { 0 };
    CHECK(load_config(&cfg, __FILE__, &untouched));
    CHECK(untouched.kind == ERR_NONE);
    CHECK(cfg.len > 0);
    str_free(&cfg);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(zero_err_is_none);
    RUN(set_fills_every_part);
    RUN(set_cuts_a_long_message);
    RUN(null_err_is_ignored);
    RUN(os_maps_codes_to_kinds);
    RUN(os_writes_op_subject_and_reason);
    RUN(load_config_reports_why);
    return TEST_REPORT();
}
