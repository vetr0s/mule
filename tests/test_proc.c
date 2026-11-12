/* Tests for src/proc/proc.c.
 *
 * Bracketed names in comment pairs mark regions that src/proc/proc.h pulls
 * into the Doxygen reference with @snippet. See tests/test_str.c for the
 * rules. The tests run sh, printf, cat and friends from PATH.
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

TEST(run_and_report) {
    /* [proc] */
    Cmd cmd = { 0 };
    Err err = { 0 };
    bool ok = cmd_append(&cmd, "sh", "-c", "exit 0") && cmd_run(&cmd, &err);
    if (!ok) log_error("%s", err.msg); /* such as "sh exited with code 3" */
    arr_free(&cmd);
    /* [proc] */
    CHECK(ok);
    CHECK(err.kind == ERR_NONE);

    Cmd three = { 0 };
    REQUIRE(cmd_append(&three, "sh", "-c", "exit 3"));
    CHECK(!cmd_run(&three, &err));
    CHECK(err.kind == ERR_EXIT);
    CHECK_EQ_INT(err.code, 3);
    CHECK_EQ_STR(err.msg, "sh exited with code 3");
    arr_free(&three);
}

TEST(signals_missing_programs_and_empty_commands) {
    Cmd c = { 0 };
    Err err = { 0 };
    REQUIRE(cmd_append(&c, "sh", "-c", "kill -9 $$"));
    CHECK(!cmd_run(&c, &err));
    CHECK(err.kind == ERR_SIGNAL);
    CHECK_EQ_INT(err.code, 9);
    CHECK_EQ_STR(err.msg, "sh was killed by signal 9");

    arr_clear(&c);
    REQUIRE(cmd_append(&c, "mule-no-such-program", "x"));
    err = (Err){ 0 };
    CHECK(!cmd_run(&c, &err));
    CHECK(err.kind == ERR_NOT_FOUND);
    CHECK(strncmp(err.msg, "run mule-no-such-program: ", 26) == 0);

    arr_clear(&c);
    CHECK(!cmd_run(&c, &err));
    CHECK(err.kind == ERR_INVALID);

    Proc none = { 0 };
    CHECK(!proc_wait(&none, &err));
    CHECK(err.kind == ERR_INVALID);
    arr_free(&c);
}

TEST(render_quotes_what_the_shell_would_split) {
    Cmd c = { 0 };
    REQUIRE(cmd_append(&c, "cc", "-o", "a b", "it's", "", "$HOME", "x=1,y"));
    Str line = { 0 };
    REQUIRE(cmd_render(&line, &c));
    CHECK_EQ_STR(str_cstr(&line), "cc -o 'a b' 'it'\\''s' '' '$HOME' x=1,y");

    /* The shell parses the line back into the same arguments. */
    Str script = { 0 }, got = { 0 };
    Err err = { 0 };
    REQUIRE(str_appendf(&script, "printf '[%%s]' %s", str_cstr(&line)));
    Cmd sh = { 0 };
    REQUIRE(cmd_append(&sh, "sh", "-c", str_cstr(&script)));
    CHECK(cmd_capture(&sh, &got, &err));
    CHECK_EQ_STR(str_cstr(&got), "[cc][-o][a b][it's][][$HOME][x=1,y]");

    size_t len = line.len;
    Cmd empty = { 0 };
    test_fail_alloc_after(0);
    CHECK(!cmd_render(&line, &c));
    CHECK(!cmd_append(&empty, "more"));
    test_fail_alloc_after(-1);
    CHECK_EQ_INT(line.len, len);
    CHECK(empty.data == NULL && empty.len == 0);
    str_free(&line);
    str_free(&script);
    str_free(&got);
    arr_free(&c);
    arr_free(&sh);
}

TEST(redirects_streams_to_files) {
    fresh();
    char in[512], out[512], err_path[512], both[512];
    REQUIRE(put("in", "from a file\n"));
    Cmd c = { 0 };
    REQUIRE(cmd_append(&c, "sh", "-c", "cat; echo oops >&2"));
    CmdIo io = { at(in, "in"), at(out, "out"), at(err_path, "err") };
    Proc p = { 0 };
    Err err = { 0 };
    REQUIRE(proc_start(&p, &c, &io, &err));
    CHECK(p.id_ != 0);
    CHECK(proc_wait(&p, &err));
    CHECK(p.id_ == 0);

    Str text = { 0 };
    CHECK(fs_read(&text, out, &err));
    CHECK_EQ_STR(str_cstr(&text), "from a file\n");
    str_clear(&text);
    CHECK(fs_read(&text, err_path, &err));
    CHECK_EQ_STR(str_cstr(&text), "oops\n");

    arr_clear(&c);
    REQUIRE(cmd_append(&c, "sh", "-c", "echo a; echo b >&2; echo c"));
    io = (CmdIo){ .out = at(both, "both"), .err = both };
    REQUIRE(proc_start(&p, &c, &io, &err));
    CHECK(proc_wait(&p, &err));
    str_clear(&text);
    CHECK(fs_read(&text, both, &err));
    CHECK_EQ_STR(str_cstr(&text), "a\nb\nc\n");

    io = (CmdIo){ .in = at(in, "missing") };
    CHECK(!proc_start(&p, &c, &io, &err));
    CHECK(err.kind == ERR_NOT_FOUND);
    CHECK(strstr(err.msg, "missing") != NULL);
    CHECK(p.id_ == 0);
    str_free(&text);
    arr_free(&c);
}

TEST(capture_appends_stdout) {
    Cmd c = { 0 };
    Str out = { 0 };
    Err err = { 0 };
    REQUIRE(str_from_cstr(&out, "> "));
    REQUIRE(cmd_append(&c, "printf", "%s-%s", "a", "b"));
    CHECK(cmd_capture(&c, &out, &err));
    CHECK_EQ_STR(str_cstr(&out), "> a-b");

    arr_clear(&c);
    REQUIRE(cmd_append(&c, "sh", "-c", "echo partial; exit 1"));
    CHECK(!cmd_capture(&c, &out, &err));
    CHECK(err.kind == ERR_EXIT);
    CHECK_EQ_STR(str_cstr(&out), "> a-b");

    /* More than a pipe holds, so the child blocks unless we read first. */
    arr_clear(&c);
    REQUIRE(cmd_append(&c, "sh", "-c",
                       "i=0; while [ $i -lt 20000 ]; do "
                       "echo 0123456789; i=$((i+1)); done"));
    str_clear(&out);
    CHECK(cmd_capture(&c, &out, &err));
    CHECK_EQ_INT(out.len, 220000);
    str_free(&out);
    arr_free(&c);
}

TEST(pool_caps_jobs_and_reports_failures) {
    fresh();
    char script[512 + 128], counts[512];
    at(counts, "counts");
    /* Each job counts the jobs running beside it, itself included. */
    snprintf(script, sizeof script,
             "cd '%s' && mkdir -p run && touch run/$$ && "
             "ls run | wc -l >> counts && sleep 0.2 && rm run/$$",
             root_);
    Cmd c = { 0 };
    REQUIRE(cmd_append(&c, "sh", "-c", script));
    ProcPool pool = { .max = 2 };
    Err err = { 0 };
    for (int i = 0; i < 6; i++) CHECK(proc_pool_start(&pool, &c, &err));
    CHECK(pool.running.len <= 2);
    CHECK(proc_pool_wait(&pool, &err));
    CHECK_EQ_INT(pool.running.len, 0);

    Str text = { 0 };
    REQUIRE(fs_read(&text, counts, &err));
    StrView rest = str_view(&text), line;
    int lines = 0;
    uint64_t most = 0;
    while (sv_split_next(&rest, SV("\n"), &line)) {
        uint64_t n = 0;
        CHECK(sv_parse_u64(sv_trim(line), 10, &n));
        if (n > most) most = n;
        lines++;
    }
    CHECK_EQ_INT(lines, 6);
    CHECK(most >= 1 && most <= 2);
    str_free(&text);

    /* A failed job stops the next start, and the rest can still be waited. */
    Cmd bad = { 0 }, ok = { 0 };
    REQUIRE(cmd_append(&bad, "false") && cmd_append(&ok, "true"));
    pool.max = 1;
    CHECK(proc_pool_start(&pool, &bad, &err));
    err = (Err){ 0 };
    CHECK(!proc_pool_start(&pool, &ok, &err));
    CHECK(err.kind == ERR_EXIT);
    CHECK_EQ_STR(err.msg, "false exited with code 1");
    CHECK_EQ_INT(pool.running.len, 0);

    CHECK(proc_pool_start(&pool, &ok, &err));
    pool.max = 0; /* the CPU count, which is at least 1 */
    CHECK(proc_pool_start(&pool, &bad, &err));
    err = (Err){ 0 };
    CHECK(!proc_pool_wait(&pool, &err));
    CHECK(err.kind == ERR_EXIT);
    CHECK_EQ_STR(err.msg, "false exited with code 1");

    CHECK(proc_pool_start(&pool, &ok, &err));
    proc_pool_free(&pool); /* waits for it */
    proc_pool_free(&pool);
    arr_free(&c);
    arr_free(&bad);
    arr_free(&ok);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(run_and_report);
    RUN(signals_missing_programs_and_empty_commands);
    RUN(render_quotes_what_the_shell_would_split);
    RUN(redirects_streams_to_files);
    RUN(capture_appends_stdout);
    RUN(pool_caps_jobs_and_reports_failures);
    scratch_done();
    return TEST_REPORT();
}
