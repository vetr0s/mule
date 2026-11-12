/* Tests for src/build/build.c.
 *
 * self_rebuild compiles small programs against dist/mule.h with $CC or cc,
 * so it needs a compiler on PATH and the current directory at the repo root,
 * as `./build test` runs it.
 */
#include "test/test.h"

#include "arr/arr.c"
#include "build/build.c"
#include "err/err.c"
#include "fs/fs.c"
#include "log/log.c"
#include "mem/mem.c"
#include "proc/proc.c"
#include "str/str.c"

#include "scratch.h"

#include <fcntl.h>
#include <sys/stat.h>

/* Set the modification time of `path` to `sec` seconds and `nsec`. */
static bool set_mtime(const char *path, int64_t sec, long nsec) {
    struct timespec ts[2] = { { 0, UTIME_OMIT }, { (time_t)sec, nsec } };
    return utimensat(AT_FDCWD, path, ts, 0) == 0;
}

TEST(needs_rebuild_to_the_nanosecond) {
    fresh();
    char out[512], a[512], b[512];
    REQUIRE(put("out", "x") && put("a", "x") && put("b", "x"));
    at(out, "out");
    const char *inputs[] = { at(a, "a"), at(b, "b") };
    REQUIRE(set_mtime(out, 1700000000, 500) &&
            set_mtime(a, 1700000000, 500) &&
            set_mtime(b, 1700000000, 499));

    Err err = { 0 };
    bool needed = true;
    CHECK(build_needs_rebuild(out, inputs, 2, &needed, &err));
    CHECK(!needed); /* equal times are up to date */

    REQUIRE(set_mtime(b, 1700000000, 501));
    CHECK(build_needs_rebuild(out, inputs, 2, &needed, &err));
    CHECK(needed);

    char missing[512];
    needed = false;
    CHECK(
        build_needs_rebuild(at(missing, "missing"), inputs, 2, &needed, &err));
    CHECK(needed);

    const char *gone[] = { a, missing };
    needed = false;
    CHECK(!build_needs_rebuild(out, gone, 2, &needed, &err));
    CHECK(err.kind == ERR_NOT_FOUND);
    CHECK(!needed);
}

TEST(reads_a_depfile) {
    fresh();
    char p[512];
    REQUIRE(put("x.d", "out/x.o: src/x.c src/a\\ b.h \\\n"
                       "  src/c$$.h\n"
                       "src/a\\ b.h:\n"
                       "src/c$$.h:\n"));
    Deps d = { 0 };
    Err err = { 0 };
    REQUIRE(build_read_deps(&d, at(p, "x.d"), &err));
    REQUIRE(d.paths.len == 3);
    CHECK_EQ_STR(d.paths.data[0], "src/x.c");
    CHECK_EQ_STR(d.paths.data[1], "src/a b.h");
    CHECK_EQ_STR(d.paths.data[2], "src/c$.h");

    /* Reading again replaces the old contents. */
    REQUIRE(put("y.d", "y.o: y.c\n"));
    REQUIRE(build_read_deps(&d, at(p, "y.d"), &err));
    REQUIRE(d.paths.len == 1);
    CHECK_EQ_STR(d.paths.data[0], "y.c");

    REQUIRE(put("empty.d", ""));
    REQUIRE(build_read_deps(&d, at(p, "empty.d"), &err));
    CHECK_EQ_INT(d.paths.len, 0);

    REQUIRE(build_read_deps(&d, at(p, "y.d"), &err));
    CHECK(!build_read_deps(&d, at(p, "missing.d"), &err));
    CHECK(err.kind == ERR_NOT_FOUND);
    CHECK_EQ_INT(d.paths.len, 1); /* unchanged */
    test_fail_alloc_after(0);
    CHECK(!build_read_deps(&d, at(p, "x.d"), &err));
    test_fail_alloc_after(-1);
    CHECK_EQ_STR(d.paths.data[0], "y.c");
    build_deps_free(&d);
    build_deps_free(&d);
}

TEST(needs_rebuild_from_a_depfile) {
    fresh();
    char out[512], dep[512], src[512], hdr[512], text[1200];
    REQUIRE(put("out", "x") && put("src.c", "x") && put("h.h", "x"));
    at(out, "out");
    at(dep, "out.d");
    snprintf(text, sizeof text, "%s: %s %s\n", out, at(src, "src.c"),
             at(hdr, "h.h"));
    Err err = { 0 };
    bool needed = false;
    CHECK(build_needs_rebuild_deps(out, dep, &needed, &err));
    CHECK(needed); /* no depfile yet */

    REQUIRE(fs_write(dep, sv_from_cstr(text), &err));
    REQUIRE(set_mtime(out, 1700000000, 10) &&
            set_mtime(src, 1700000000, 5) &&
            set_mtime(hdr, 1700000000, 5));
    CHECK(build_needs_rebuild_deps(out, dep, &needed, &err));
    CHECK(!needed);

    REQUIRE(set_mtime(hdr, 1700000000, 11));
    CHECK(build_needs_rebuild_deps(out, dep, &needed, &err));
    CHECK(needed);

    REQUIRE(set_mtime(hdr, 1700000000, 5));
    REQUIRE(fs_remove(hdr, &err));
    needed = false;
    CHECK(build_needs_rebuild_deps(out, dep, &needed, &err));
    CHECK(needed); /* a deleted header means rebuild, not fail */
}

TEST(run_logs_the_quoted_command) {
    fresh();
    char p[512];
    FILE *log = fopen(at(p, "log"), "w+");
    REQUIRE(log != NULL);
    log_set_output(log);
    Cmd c = { 0 };
    Err err = { 0 };
    REQUIRE(cmd_append(&c, "sh", "-c", "exit 0"));
    CHECK(build_run(&c, &err));
    ProcPool pool = { 0 };
    CHECK(build_start(&pool, &c, &err) && proc_pool_wait(&pool, &err));
    proc_pool_free(&pool);
    log_set_output(NULL);
    fclose(log);

    Str text = { 0 };
    REQUIRE(fs_read(&text, p, &err));
    StrView rest = str_view(&text), line;
    int lines = 0;
    while (sv_split_next(&rest, SV("\n"), &line)) {
        CHECK(sv_ends_with(line, SV(": sh -c 'exit 0'")));
        lines++;
    }
    CHECK_EQ_INT(lines, 2);
    str_free(&text);
    arr_free(&c);
}

TEST(compdb_writes_json_once) {
    fresh();
    char p[512], cwd[PATH_MAX];
    REQUIRE(getcwd(cwd, sizeof cwd) != NULL);
    CompDb db = { 0 };
    Cmd c = { 0 };
    REQUIRE(cmd_append(&c, "cc", "-DMSG=\"hi\\n\"", "-c", "a.c"));
    REQUIRE(build_compdb_add(&db, "a.c", &c));
    arr_clear(&c);
    REQUIRE(cmd_append(&c, "cc", "-c", "b.c"));
    REQUIRE(build_compdb_add(&db, "b.c", &c));

    Err err = { 0 };
    REQUIRE(build_compdb_write(&db, at(p, "compile_commands.json"), &err));
    Str want = { 0 }, got = { 0 };
    REQUIRE(str_appendf(
        &want,
        "[\n"
        "  {\"directory\": \"%s\", \"file\": \"a.c\", \"arguments\": "
        "[\"cc\", \"-DMSG=\\\"hi\\\\n\\\"\", \"-c\", \"a.c\"]},\n"
        "  {\"directory\": \"%s\", \"file\": \"b.c\", \"arguments\": "
        "[\"cc\", \"-c\", \"b.c\"]}\n"
        "]\n",
        cwd, cwd));
    REQUIRE(fs_read(&got, p, &err));
    CHECK_EQ_STR(str_cstr(&got), str_cstr(&want));

    /* The same text again leaves the file, and its time, alone. */
    REQUIRE(set_mtime(p, 1700000000, 0));
    REQUIRE(build_compdb_write(&db, p, &err));
    FsStat st = { 0 };
    REQUIRE(fs_stat(p, &st, &err));
    CHECK(st.mtime_ns == INT64_C(1700000000000000000));

    size_t len = db.json.len;
    test_fail_alloc_after(0);
    CHECK(!build_compdb_add(&db, "a-much-longer-file-name.c", &c));
    test_fail_alloc_after(-1);
    CHECK_EQ_INT(db.json.len, len);
    str_free(&want);
    str_free(&got);
    build_compdb_free(&db);
    build_compdb_free(&db);
    arr_free(&c);
}

/* Write `prog.c`, which prints `version` and exits 7. */
static bool write_prog(const char *version) {
    char src[512], cwd[PATH_MAX];
    if (getcwd(cwd, sizeof cwd) == NULL) return false;
    Str text = { 0 };
    bool ok = str_appendf(&text,
                          "#define MULE_IMPLEMENTATION\n"
                          "#include \"%s/dist/mule.h\"\n"
                          "int main(int argc, char **argv) {\n"
                          "    MULE_REBUILD_SELF(argc, argv);\n"
                          "    printf(\"%s %%s\\n\", argc > 1 ? argv[1] : \"\");\n"
                          "    return 7;\n"
                          "}\n",
                          cwd, version) &&
              fs_write(at(src, "prog.c"), str_view(&text), NULL);
    str_free(&text);
    return ok;
}

/* Run the program from the scratch directory. Return its exit code and
 * put its stdout in `out`. */
static int run_prog(Str *out) {
    char o[512], e[512];
    Cmd c = { 0 };
    Proc p = { 0 };
    Err err = { 0 };
    char script[600];
    snprintf(script, sizeof script, "cd '%s' && exec ./prog arg", root_);
    CmdIo io = { .out = at(o, "out"), .err = at(e, "err") };
    if (!cmd_append(&c, "sh", "-c", script) || !proc_start(&p, &c, &io, &err))
        return -1;
    int code = proc_wait(&p, &err) ? 0 : err.code;
    str_clear(out);
    if (!fs_read(out, o, NULL)) code = -1;
    arr_free(&c);
    return code;
}

TEST(self_rebuild) {
    fresh();
    char src[512], exe[512], old[512];
    Str out = { 0 };
    Err err = { 0 };
    REQUIRE(write_prog("v1"));
    const char *compiler = getenv("CC") != NULL && getenv("CC")[0] != '\0'
                               ? getenv("CC")
                               : "cc";
    Cmd boot = { 0 };
    REQUIRE(
        cmd_append(&boot, compiler, "-o", at(exe, "prog"), at(src, "prog.c")));
    REQUIRE(cmd_run(&boot, &err));
    arr_free(&boot);

    /* No depfile yet, so the first run rebuilds once to get one. */
    CHECK_EQ_INT(run_prog(&out), 7);
    CHECK_EQ_STR(str_cstr(&out), "v1 arg\n");
    CHECK(fs_exists(at(old, ".build/prog.d")));

    REQUIRE(write_prog("v2"));
    CHECK_EQ_INT(run_prog(&out), 7);
    CHECK_EQ_STR(str_cstr(&out), "v2 arg\n");

    /* Up to date: nothing is compiled, so nothing is logged. */
    Str log = { 0 };
    CHECK_EQ_INT(run_prog(&out), 7);
    CHECK(fs_read(&log, at(old, "err"), &err));
    CHECK_EQ_STR(str_cstr(&log), "");

    /* A broken edit leaves the last good program in place. */
    REQUIRE(fs_write(src, SV("this is not C\n"), &err));
    CHECK_EQ_INT(run_prog(&out), 1);
    CHECK(fs_exists(exe));
    /* Put the source back, and date the program after it: no rebuild. */
    REQUIRE(write_prog("v2"));
    REQUIRE(set_mtime(exe, 4000000000, 0));
    CHECK_EQ_INT(run_prog(&out), 7);
    CHECK_EQ_STR(str_cstr(&out), "v2 arg\n");
    str_free(&out);
    str_free(&log);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(needs_rebuild_to_the_nanosecond);
    RUN(reads_a_depfile);
    RUN(needs_rebuild_from_a_depfile);
    RUN(run_logs_the_quoted_command);
    RUN(compdb_writes_json_once);
    RUN(self_rebuild);
    scratch_done();
    return TEST_REPORT();
}
