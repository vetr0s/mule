/* Tests for src/args/args.c.
 *
 * Bracketed names in comment pairs mark regions that src/args/args.h pulls
 * into the Doxygen reference with @snippet. See tests/test_str.c for the
 * rules.
 */
#include "test/test.h"

#include "args/args.c"
#include "arr/arr.c"
#include "err/err.c"
#include "log/log.c"
#include "mem/mem.c"
#include "str/str.c"

/* The command line of a build program, as the tests use it. */
typedef struct Opts {
    bool verbose, quiet, no_sanitize;
    const char *out;
    uint64_t jobs;
} Opts;

static Args args_for(Opts *o, ArgOpt opts[5]) {
    static const ArgCmd cmds[] = {
        { "debug", "debug build of main (default)" },
        { "test", "build and run the tests" },
    };
    opts[0] = (ArgOpt){ "verbose",   'v',  ARG_FLAG,
                        &o->verbose, NULL, "print each command" };
    opts[1] = (ArgOpt){ NULL, 'q', ARG_FLAG, &o->quiet, NULL, "print less" };
    opts[2] = (ArgOpt){ "no-sanitize",   0,    ARG_FLAG,
                        &o->no_sanitize, NULL, "build without sanitizers" };
    opts[3] = (ArgOpt){
        "out", 'o', ARG_STR, &o->out, "DIR", "where output goes"
    };
    opts[4] = (ArgOpt){
        "jobs", 'j', ARG_U64, &o->jobs, "N", "compile N at once"
    };
    return (Args){ .usage = "build [command] [options] [names...]",
                   .opts = opts,
                   .opt_count = 5,
                   .cmds = cmds,
                   .cmd_count = 2 };
}

/* main receives a mutable argv. Tests hold literals, which are const. */
static char **mutable(const char *const *argv) {
    return (char **)(uintptr_t)argv;
}

static bool parse(Args *a, Err *err, int argc, const char *const *argv) {
    return args_parse(a, argc, mutable(argv), err);
}

TEST(parses_every_form) {
    static const char *const line[] = { "build", "-v", "--out=dist",
                                        "-j",    "8",  "str" };
    int argc = 6;
    char **argv = mutable(line);
    /* [args] */
    bool verbose = false;
    const char *out = ".build";
    uint64_t jobs = 0;
    const ArgOpt opts[] = {
        { "verbose", 'v', ARG_FLAG, &verbose, NULL, "print each command" },
        { "out", 'o', ARG_STR, &out, "DIR", "where output goes" },
        { "jobs", 'j', ARG_U64, &jobs, "N", "compile N at once" },
    };
    Args a = { .usage = "build [options] [names...]",
               .opts = opts,
               .opt_count = sizeof opts / sizeof opts[0] };
    Err err = { 0 };
    if (!args_parse(&a, argc, argv, &err)) log_error("%s", err.msg);
    /* [args] */
    CHECK(verbose);
    CHECK_EQ_STR(out, "dist");
    CHECK_EQ_INT(jobs, 8);
    REQUIRE(a.rest.len == 1);
    CHECK_EQ_STR(a.rest.data[0], "str");
    args_free(&a);
}

TEST(long_short_grouped_and_double_dash) {
    Opts o = { 0 };
    ArgOpt opts[5];
    Args a = args_for(&o, opts);
    Err err = { 0 };
    const char *argv[] = { "build", "test",          "--out", "x",  "-vqj4",
                           "a",     "--no-sanitize", "--",    "-b", "--out=y" };
    REQUIRE(parse(&a, &err, 10, argv));
    CHECK_EQ_STR(a.cmd, "test");
    CHECK(o.verbose && o.quiet && o.no_sanitize);
    CHECK_EQ_STR(o.out, "x");
    CHECK_EQ_INT(o.jobs, 4);
    REQUIRE(a.rest.len == 3);
    CHECK_EQ_STR(a.rest.data[0], "a");
    CHECK_EQ_STR(a.rest.data[1], "-b");
    CHECK_EQ_STR(a.rest.data[2], "--out=y");
    CHECK(!a.help);
    args_free(&a);

    a = args_for(&o, opts);
    const char *argv2[] = { "build", "-ox", "debug", "-", "-h" };
    REQUIRE(parse(&a, &err, 5, argv2));
    CHECK_EQ_STR(o.out, "x");
    CHECK_EQ_STR(a.cmd, "debug");
    REQUIRE(a.rest.len == 1);
    CHECK_EQ_STR(a.rest.data[0], "-"); /* stdin, by convention */
    CHECK(a.help);
    args_free(&a);
}

TEST(errors_change_nothing) {
    static const struct {
        int argc;
        const char *argv[4];
        const char *msg;
    } cases[] = {
        { 2, { "b", "--nope" }, "unknown option '--nope'" },
        { 2, { "b", "-vx" }, "unknown option '-x' in '-vx'" },
        { 2, { "b", "--out" }, "--out needs a value" },
        { 2, { "b", "-j" }, "-j needs a value" },
        { 2, { "b", "--verbose=yes" }, "--verbose takes no value" },
        { 3,
          { "b", "--jobs", "many" },
          "--jobs: expected a number, got 'many'" },
        { 2,
          { "b", "-j18446744073709551616" },
          "--jobs: expected a number, got '18446744073709551616'" },
        { 2, { "b", "build" }, "unknown command 'build'" },
        { 4, { "b", "-v", "x", "--nope" }, "unknown command 'x'" },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        Opts o = { .out = "keep", .jobs = 7 };
        ArgOpt opts[5];
        Args a = args_for(&o, opts);
        Err err = { 0 };
        CHECK(!parse(&a, &err, cases[i].argc, cases[i].argv));
        CHECK(err.kind == ERR_INVALID);
        CHECK_EQ_STR(err.msg, cases[i].msg);
        CHECK(!o.verbose && o.jobs == 7);
        CHECK_EQ_STR(o.out, "keep");
        CHECK(a.cmd == NULL && a.rest.len == 0 && a.rest.data == NULL);
    }
}

TEST(out_of_memory_changes_nothing) {
    Opts o = { 0 };
    ArgOpt opts[5];
    Args a = args_for(&o, opts);
    Err err = { 0 };
    const char *argv[] = { "build", "-v", "debug", "a", "b" };
    test_fail_alloc_after(0);
    CHECK(!parse(&a, &err, 5, argv));
    test_fail_alloc_after(-1);
    CHECK(err.kind == ERR_NO_MEMORY);
    CHECK(!o.verbose && a.cmd == NULL && a.rest.len == 0);
}

TEST(zeroed_args_takes_positionals) {
    Args a = { 0 };
    Err err = { 0 };
    const char *argv[] = { "prog", "x", "y" };
    REQUIRE(parse(&a, &err, 3, argv));
    CHECK_EQ_INT(a.rest.len, 2);
    const char *bad[] = { "prog", "--x" };
    CHECK(!parse(&a, &err, 2, bad));
    CHECK_EQ_INT(a.rest.len, 2);
    args_free(&a);
    args_free(&a);
}

TEST(help_lists_commands_and_options) {
    Opts o = { 0 };
    ArgOpt opts[5];
    Args a = args_for(&o, opts);
    Str help = { 0 };
    REQUIRE(args_help(&help, &a));
    CHECK_EQ_STR(str_cstr(&help),
                 "usage: build [command] [options] [names...]\n"
                 "\n"
                 "commands:\n"
                 "  debug              debug build of main (default)\n"
                 "  test               build and run the tests\n"
                 "\n"
                 "options:\n"
                 "  -v, --verbose      print each command\n"
                 "  -q                 print less\n"
                 "      --no-sanitize  build without sanitizers\n"
                 "  -o, --out=DIR      where output goes\n"
                 "  -j, --jobs=N       compile N at once\n"
                 "  -h, --help         show this help\n");

    size_t len = help.len;
    test_fail_alloc_after(0);
    CHECK(!args_help(&help, &a));
    test_fail_alloc_after(-1);
    CHECK_EQ_INT(help.len, len);
    str_free(&help);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(parses_every_form);
    RUN(long_short_grouped_and_double_dash);
    RUN(errors_change_nothing);
    RUN(out_of_memory_changes_nothing);
    RUN(zeroed_args_takes_positionals);
    RUN(help_lists_commands_and_options);
    return TEST_REPORT();
}
