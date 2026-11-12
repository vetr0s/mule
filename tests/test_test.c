/* Tests for src/test/test.h, the harness itself.
 *
 * A failure planted here would fail this file too. So the program runs
 * itself on a test that fails on purpose, which main only runs when it is
 * named, and checks what that run printed.
 */
#include "test/test.h"

#include "arr/arr.c"
#include "err/err.c"
#include "fs/fs.c"
#include "log/log.c"
#include "mem/mem.c"
#include "proc/proc.c"
#include "str/str.c"

#include "scratch.h"

static const char *self_;

enum { PLANTED_LINE = __LINE__ + 2 };
TEST(fails_on_purpose) {
    CHECK(100 % 7 == 0);
}

TEST(leaks_on_purpose) {
    void *p = mem_realloc(NULL, NULL, 0, 8);
    CHECK(p != NULL);
}

/* Run this program on the test `name`, with GITHUB_ACTIONS set to `ci` or
 * unset when it is null, and append what it printed to `out`. */
static bool run_planted(Str *out, const char *name, const char *ci) {
    char path[512];
    Cmd c = { 0 };
    CmdIo io = { .out = at(path, "out") };
    Proc p = { 0 };
    bool ok = (ci != NULL ? setenv("GITHUB_ACTIONS", ci, 1)
                          : unsetenv("GITHUB_ACTIONS")) == 0 &&
              cmd_append(&c, self_, name) &&
              proc_start(&p, &c, &io, NULL);
    bool passed = ok && proc_wait(&p, NULL);
    arr_free(&c);
    return ok && !passed && fs_read(out, path, NULL);
}

TEST(annotates_a_failed_check_on_github) {
    fresh();
    Str out = { 0 };
    REQUIRE(run_planted(&out, "fails_on_purpose", "true"));
    char want[256];
    snprintf(want, sizeof want,
             "::error file=%s,line=%d,title=fails_on_purpose::"
             "CHECK(100 %%25 7 == 0) failed\n",
             __FILE__, (int)PLANTED_LINE);
    CHECK(strstr(str_cstr(&out), want) != NULL);
    snprintf(want, sizeof want, "  %s:%d: CHECK(100 %% 7 == 0) failed\n",
             __FILE__, (int)PLANTED_LINE);
    CHECK(strstr(str_cstr(&out), want) != NULL);
    str_free(&out);
}

static int leak_run_line_;

TEST(annotates_a_leak_at_its_run) {
    fresh();
    Str out = { 0 };
    REQUIRE(run_planted(&out, "leaks_on_purpose", "true"));
    char want[256];
    snprintf(want, sizeof want,
             "::error file=%s,line=%d,title=leaks_on_purpose::"
             "1 allocation(s) leaked\n",
             __FILE__, leak_run_line_);
    CHECK(strstr(str_cstr(&out), want) != NULL);
    str_free(&out);
}

TEST(prints_no_annotation_off_github) {
    fresh();
    Str out = { 0 };
    REQUIRE(run_planted(&out, "fails_on_purpose", NULL));
    CHECK(strstr(str_cstr(&out), "::error") == NULL);
    CHECK(strstr(str_cstr(&out), "[FAIL] fails_on_purpose") != NULL);
    str_free(&out);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    self_ = argv[0];
    if (argc > 1) RUN(fails_on_purpose);
    leak_run_line_ = __LINE__ + 1;
    if (argc > 1) RUN(leaks_on_purpose);
    RUN(annotates_a_failed_check_on_github);
    RUN(annotates_a_leak_at_its_run);
    RUN(prints_no_annotation_off_github);
    scratch_done();
    return TEST_REPORT();
}
