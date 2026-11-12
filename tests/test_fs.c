/* Tests for src/fs/fs.c.
 *
 * Bracketed names in comment pairs mark regions that src/fs/fs.h pulls into
 * the Doxygen reference with @snippet. See tests/test_str.c for the rules.
 *
 * Each test works in a fresh directory from tests/scratch.h.
 */
#include "test/test.h"

#include "arr/arr.c"
#include "err/err.c"
#include "fs/fs.c"
#include "log/log.c"
#include "mem/mem.c"
#include "str/str.c"

#include "scratch.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK_SV(v, lit) CHECK_EQ_MEM((v).data, (v).len, lit, sizeof(lit) - 1)

TEST(write_read_and_atomic) {
    fresh();
    char path[512];
    at(path, "today.txt");
    /* [fs] */
    Err err = { 0 };
    Str text = { 0 };
    bool ok = fs_write_atomic(path, SV("hello\n"), &err) &&
              fs_read(&text, path, &err);
    if (!ok) log_error("%s", err.msg);
    /* [fs] */
    CHECK(ok);
    CHECK_EQ_STR(str_cstr(&text), "hello\n");

    CHECK(fs_write(path, SV("more"), &err));
    CHECK(fs_read(&text, path, &err)); /* appends */
    CHECK_EQ_STR(str_cstr(&text), "hello\nmore");
    str_free(&text);
}

TEST(read_failures_leave_out_unchanged) {
    fresh();
    char p[512];
    Str out = { 0 };
    REQUIRE(str_from_cstr(&out, "keep"));
    Err err = { 0 };
    CHECK(!fs_read(&out, at(p, "missing"), &err));
    CHECK(err.kind == ERR_NOT_FOUND);
    CHECK(strstr(err.msg, "missing") != NULL);
    CHECK_EQ_STR(str_cstr(&out), "keep");

    REQUIRE(put("big", "0123456789abcdefghijklmnopqrstuvwxyz"));
    err = (Err){ 0 };
    test_fail_alloc_after(0);
    CHECK(!fs_read(&out, at(p, "big"), &err));
    test_fail_alloc_after(-1);
    CHECK(err.kind == ERR_NO_MEMORY);
    CHECK_EQ_STR(str_cstr(&out), "keep");
    str_free(&out);
}

TEST(atomic_write_keeps_mode_and_fails_cleanly) {
    fresh();
    char p[512];
    Err err = { 0 };
    FsStat st = { 0 };
    REQUIRE(fs_write_atomic(at(p, "new"), SV("x"), &err));
    REQUIRE(fs_stat(p, &st, &err));
    CHECK_EQ_INT(st.mode, 0644);

    REQUIRE(put("old", "old"));
    REQUIRE(chmod(at(p, "old"), 0600) == 0);
    REQUIRE(fs_write_atomic(p, SV("new"), &err));
    REQUIRE(fs_stat(p, &st, &err));
    CHECK_EQ_INT(st.mode, 0600);

    /* A directory in the way: the rename fails and the temp file goes. */
    REQUIRE(fs_mkdir_p(at(p, "dir"), &err));
    CHECK(!fs_write_atomic(p, SV("x"), &err));
    CHECK(err.kind != ERR_NONE);
    CHECK(fs_remove(p, &err)); /* still empty */

    err = (Err){ 0 };
    CHECK(!fs_write_atomic(at(p, "no/such/dir/f"), SV("x"), &err));
    CHECK(err.kind == ERR_NOT_FOUND);
}

TEST(stat_follows_and_lstat_does_not) {
    fresh();
    char f[512], l[512], d[512];
    Err err = { 0 };
    REQUIRE(put("f", "hello"));
    REQUIRE(symlink(at(f, "f"), at(l, "l")) == 0);
    REQUIRE(symlink("nowhere", at(d, "dangling")) == 0);

    FsStat st = { 0 };
    REQUIRE(fs_stat(l, &st, &err));
    CHECK(st.type == FS_FILE);
    CHECK_EQ_INT(st.size, 5);
    CHECK(st.mtime_ns > 0);
    REQUIRE(fs_lstat(l, &st, &err));
    CHECK(st.type == FS_LINK);
    REQUIRE(fs_stat(root_, &st, &err));
    CHECK(st.type == FS_DIR);

    FsStat untouched = { .size = 99 };
    CHECK(!fs_stat(at(f, "missing"), &untouched, &err));
    CHECK(err.kind == ERR_NOT_FOUND);
    CHECK_EQ_INT(untouched.size, 99);

    CHECK(fs_exists(l));
    CHECK(!fs_exists(d));
    CHECK(!fs_exists(f));
}

TEST(mtime_has_nanoseconds) {
    fresh();
    char p[512];
    REQUIRE(put("f", "x"));
    struct timespec ts[2] = { { 0, UTIME_OMIT }, { 1700000000, 123456789 } };
    REQUIRE(utimensat(AT_FDCWD, at(p, "f"), ts, 0) == 0);
    FsStat st = { 0 };
    REQUIRE(fs_stat(p, &st, NULL));
    CHECK(st.mtime_ns == INT64_C(1700000000123456789));
}

TEST(mkdir_p_makes_parents_and_reports_files_in_the_way) {
    fresh();
    char p[512];
    Err err = { 0 };
    CHECK(fs_mkdir_p(at(p, "a/b/c"), &err));
    CHECK(fs_mkdir_p(p, &err)); /* already there */
    CHECK(fs_mkdir_p(at(p, "a//b/d/"), &err));
    FsStat st = { 0 };
    CHECK(fs_stat(at(p, "a/b/d"), &st, &err) && st.type == FS_DIR);

    REQUIRE(put("file", "x"));
    CHECK(!fs_mkdir_p(at(p, "file"), &err));
    CHECK(err.kind == ERR_EXISTS);
    err = (Err){ 0 };
    CHECK(!fs_mkdir_p(at(p, "file/sub"), &err));
    CHECK(err.kind == ERR_NOT_DIR);
    CHECK(strstr(err.msg, "/file:") != NULL);
}

TEST(remove_and_remove_all) {
    fresh();
    char p[512], out[512];
    Err err = { 0 };
    REQUIRE(put("f", "x"));
    CHECK(fs_remove(at(p, "f"), &err));
    CHECK(!fs_exists(p));
    CHECK(!fs_remove(p, &err));
    CHECK(err.kind == ERR_NOT_FOUND);

    REQUIRE(fs_mkdir_p(at(p, "t/a/b"), &err));
    REQUIRE(put("t/a/b/f", "x"));
    REQUIRE(put("t/g", "x"));
    REQUIRE(fs_mkdir_p(at(out, "outside"), &err));
    REQUIRE(put("outside/keep", "x"));
    REQUIRE(symlink(out, at(p, "t/link")) == 0);

    CHECK(!fs_remove(at(p, "t"), &err)); /* not empty */
    CHECK(fs_remove_all(p, &err));
    CHECK(!fs_exists(p));
    CHECK(fs_exists(at(out, "outside/keep"))); /* the link was not followed */
    CHECK(fs_remove_all(at(p, "never"), &err));
}

TEST(rename_and_copy) {
    fresh();
    char a[512], b[512], c[512];
    Err err = { 0 };
    REQUIRE(put("a", "data"));
    REQUIRE(chmod(at(a, "a"), 0750) == 0);
    CHECK(fs_copy(a, at(b, "b"), &err));
    Str text = { 0 };
    CHECK(fs_read(&text, b, &err));
    CHECK_EQ_STR(str_cstr(&text), "data");
    str_free(&text);
    FsStat st = { 0 };
    CHECK(fs_stat(b, &st, &err) && st.mode == 0750);

    CHECK(fs_rename(b, at(c, "c"), &err));
    CHECK(fs_exists(c));
    CHECK(!fs_exists(b));
    CHECK(!fs_rename(at(b, "missing"), a, &err));
    CHECK(err.kind == ERR_NOT_FOUND);
    err = (Err){ 0 };
    CHECK(!fs_copy(at(b, "missing"), a, &err));
    CHECK(err.kind == ERR_NOT_FOUND);
}

TEST(copy_tree_copies_files_dirs_and_links) {
    fresh();
    char p[512], q[512];
    Err err = { 0 };
    REQUIRE(fs_mkdir_p(at(p, "src/sub"), &err));
    REQUIRE(put("src/sub/f", "deep"));
    REQUIRE(symlink("sub/f", at(p, "src/link")) == 0);
    CHECK(fs_copy_tree(at(p, "src"), at(q, "dst"), &err));

    Str text = { 0 };
    CHECK(fs_read(&text, at(q, "dst/sub/f"), &err));
    CHECK_EQ_STR(str_cstr(&text), "deep");
    str_clear(&text);
    FsStat st = { 0 };
    CHECK(fs_lstat(at(q, "dst/link"), &st, &err) && st.type == FS_LINK);
    CHECK(fs_read(&text, q, &err)); /* the copied link still resolves */
    CHECK_EQ_STR(str_cstr(&text), "deep");
    str_free(&text);
}

typedef struct Visits {
    Str seen;
    const char *skip, *stop;
} Visits;

static FsWalkAct record(void *ctx, const char *path, FsType type) {
    Visits *v = ctx;
    const char *rel = path + strlen(root_);
    if (!str_appendf(&v->seen, "%s%s,", rel[0] ? rel : ".",
                     type == FS_DIR    ? "/"
                     : type == FS_LINK ? "@"
                                       : ""))
        return FS_WALK_STOP;
    if (v->skip != NULL && strcmp(rel, v->skip) == 0) return FS_WALK_SKIP;
    if (v->stop != NULL && strcmp(rel, v->stop) == 0) return FS_WALK_STOP;
    return FS_WALK_NEXT;
}

TEST(walk_is_sorted_and_can_skip_or_stop) {
    fresh();
    char p[512];
    Err err = { 0 };
    REQUIRE(fs_mkdir_p(at(p, "a"), &err));
    REQUIRE(
        put("c", "x") && put("b", "x") && put("a/z", "x") && put("a/y", "x"));
    REQUIRE(symlink(at(p, "a"), at(p, "d")) == 0);

    Visits v = { 0 };
    CHECK(fs_walk(root_, record, &v, &err));
    CHECK_EQ_STR(str_cstr(&v.seen), "./,/a/,/a/y,/a/z,/b,/c,/d@,");

    str_clear(&v.seen);
    v.skip = "/a";
    CHECK(fs_walk(root_, record, &v, &err));
    CHECK_EQ_STR(str_cstr(&v.seen), "./,/a/,/b,/c,/d@,");

    str_clear(&v.seen);
    v.skip = NULL;
    v.stop = "/a/y";
    CHECK(fs_walk(root_, record, &v, &err));
    CHECK_EQ_STR(str_cstr(&v.seen), "./,/a/,/a/y,");
    str_free(&v.seen);

    CHECK(!fs_walk(at(p, "missing"), record, &v, &err));
    CHECK(err.kind == ERR_NOT_FOUND);
}

TEST(exe_path_names_this_program) {
    Str exe = { 0 };
    Err err = { 0 };
    REQUIRE(fs_exe_path(&exe, &err));
    CHECK(exe.len > 0 && exe.data[0] == '/');
    CHECK_SV(fs_basename(str_view(&exe)), "test_fs");
    FsStat st = { 0 };
    CHECK(fs_stat(str_cstr(&exe), &st, &err) && st.type == FS_FILE);
    str_free(&exe);
}

TEST(path_pieces) {
    static const struct {
        const char *path, *base, *dir, *ext;
    } cases[] = {
        { "a/b.c", "b.c", "a", ".c" },
        { "src/str/str.h", "str.h", "src/str", ".h" },
        { "a/b/", "b", "a", "" },
        { "b.tar.gz", "b.tar.gz", ".", ".gz" },
        { ".build", ".build", ".", "" },
        { "a/.build", ".build", "a", "" },
        { "a.", "a.", ".", "." },
        { "..", "..", ".", "" },
        { "/", "/", "/", "" },
        { "//", "/", "/", "" },
        { "/usr", "usr", "/", "" },
        { "a//b", "b", "a", "" },
        { "", "", ".", "" },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        StrView p = sv_from_cstr(cases[i].path);
        StrView base = fs_basename(p), dir = fs_dirname(p),
                ext = fs_extension(p);
        CHECK_EQ_MEM(base.data, base.len, cases[i].base, strlen(cases[i].base));
        CHECK_EQ_MEM(dir.data, dir.len, cases[i].dir, strlen(cases[i].dir));
        CHECK_EQ_MEM(ext.data, ext.len, cases[i].ext, strlen(cases[i].ext));
    }
}

TEST(join_adds_one_slash) {
    Str p = { 0 };
    CHECK(fs_join(&p, SV("a")) &&
          fs_join(&p, SV("b")) &&
          fs_join(&p, SV("/c")) &&
          fs_join(&p, SV("")));
    CHECK_EQ_STR(str_cstr(&p), "a/b/c");
    str_clear(&p);
    CHECK(fs_join(&p, SV("/")) &&
          fs_join(&p, SV("usr/")) &&
          fs_join(&p, SV("lib")));
    CHECK_EQ_STR(str_cstr(&p), "/usr/lib");
    str_free(&p);

    REQUIRE(str_from_cstr(&p, "abc"));
    REQUIRE(str_shrink_to_fit(&p));
    Str before = p;
    test_fail_alloc_after(0);
    CHECK(!fs_join(&p, SV("def")));
    test_fail_alloc_after(-1);
    CHECK(p.data == before.data && p.len == 3 && p.capacity == before.capacity);
    CHECK_EQ_STR(str_cstr(&p), "abc");
    str_free(&p);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(write_read_and_atomic);
    RUN(read_failures_leave_out_unchanged);
    RUN(atomic_write_keeps_mode_and_fails_cleanly);
    RUN(stat_follows_and_lstat_does_not);
    RUN(mtime_has_nanoseconds);
    RUN(mkdir_p_makes_parents_and_reports_files_in_the_way);
    RUN(remove_and_remove_all);
    RUN(rename_and_copy);
    RUN(copy_tree_copies_files_dirs_and_links);
    RUN(walk_is_sorted_and_can_skip_or_stop);
    RUN(exe_path_names_this_program);
    RUN(path_pieces);
    RUN(join_adds_one_slash);
    scratch_done();
    return TEST_REPORT();
}
