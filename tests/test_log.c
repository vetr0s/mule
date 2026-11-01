/* Tests for src/log/log.c.
 *
 * Bracketed names in comment pairs mark regions that src/log/log.h pulls into
 * the Doxygen reference with @snippet. See tests/test_str.c for the rules.
 */
#define _XOPEN_SOURCE 700 /* posix_openpt, for a real terminal to test on */
#define LOG_LEVEL_MIN LOG_DEBUG /* so log_trace compiles out in this file */

#include "test.h"

#include "log/log.c"
#include "mem/mem.c"
#include "str/str.c"

#include <fcntl.h>
#include <sys/wait.h>

/* Start each case from an environment that says nothing about colour. */
static void clear_color_env(void) {
    unsetenv("NO_COLOR");
    unsetenv("CLICOLOR_FORCE");
    unsetenv("FORCE_COLOR");
    setenv("TERM", "xterm-256color", 1);
}

/* The terminal end of a new pseudo-terminal, or null. Closes `*master` too
 * when the caller closes it with close_pty. */
static FILE *open_pty(int *master) {
    *master = posix_openpt(O_RDWR | O_NOCTTY);
    if (*master < 0) return nullptr;
    const char *name = grantpt(*master) == 0 && unlockpt(*master) == 0
                           ? ptsname(*master)
                           : nullptr;
    int fd = name != nullptr ? open(name, O_RDWR | O_NOCTTY) : -1;
    FILE *f = fd >= 0 ? fdopen(fd, "w") : nullptr;
    if (f == nullptr) {
        if (fd >= 0) close(fd);
        close(*master);
    }
    return f;
}

static void close_pty(FILE *f, int master) {
    fclose(f);
    close(master);
}

TEST(color_off_for_files_and_pipes) {
    clear_color_env();
    FILE *f = tmpfile();
    REQUIRE(f != nullptr);
    CHECK(!log_color_enabled(f));
    fclose(f);
}

TEST(color_on_for_terminals) {
    /* [color] */
    bool color = log_color_enabled(stderr);
    const char *ok = color ? LOG_GREEN "ok" LOG_RESET : "ok";
    /* [color] */
    CHECK(strcmp(ok, "ok") == 0 || strcmp(ok, LOG_GREEN "ok" LOG_RESET) == 0);

    clear_color_env();
    int master;
    FILE *tty = open_pty(&master);
    REQUIRE(tty != nullptr);
    CHECK(log_color_enabled(tty));

    setenv("TERM", "dumb", 1);
    CHECK(!log_color_enabled(tty));
    setenv("TERM", "", 1);
    CHECK(!log_color_enabled(tty));
    unsetenv("TERM");
    CHECK(!log_color_enabled(tty));
    close_pty(tty, master);
}

TEST(force_turns_color_on) {
    FILE *f = tmpfile();
    REQUIRE(f != nullptr);
    static const char *const names[] = { "CLICOLOR_FORCE", "FORCE_COLOR" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        clear_color_env();
        setenv(names[i], "1", 1);
        CHECK(log_color_enabled(f));
        setenv(names[i], "0", 1);
        CHECK(!log_color_enabled(f));
        setenv(names[i], "", 1);
        CHECK(!log_color_enabled(f));
    }
    fclose(f);
}

TEST(no_color_wins) {
    clear_color_env();
    int master;
    FILE *tty = open_pty(&master);
    REQUIRE(tty != nullptr);

    setenv("NO_COLOR", "1", 1);
    CHECK(!log_color_enabled(tty));
    setenv("CLICOLOR_FORCE", "1", 1);
    CHECK(!log_color_enabled(tty));

    /* An empty NO_COLOR does not count, per no-color.org. */
    setenv("NO_COLOR", "", 1);
    CHECK(log_color_enabled(tty));
    close_pty(tty, master);
}

/* --- Messages ----------------------------------------------------------- */

/* Send log output to a fresh temporary file with colour off. */
static FILE *capture(void) {
    clear_color_env();
    FILE *f = tmpfile();
    log_set_output(f);
    return f;
}

/* Everything written to `f`, read into `out`. Closes `f` and puts the logger
 * back on stderr at the default level. */
static bool release(FILE *f, Str *out) {
    log_set_output(nullptr);
    log_set_level(LOG_INFO);
    fflush(f);
    rewind(f);
    bool ok = str_append_file(out, f);
    fclose(f);
    return ok;
}

TEST(writes_level_location_message) {
    FILE *f = capture();
    REQUIRE(f != nullptr);
    /* [log] */
    log_set_level(LOG_TRACE);
    log_info("loaded %d records", 3);
    log_debug("cache hit rate %.2f", 0.75);
    log_trace("compiled out here, by LOG_LEVEL_MIN");
    /* [log] */
    int line = __LINE__ - 4;

    Str got = {}, want = {};
    REQUIRE(release(f, &got));
    REQUIRE(str_appendf(&want,
                        "INFO  %s:%d: loaded 3 records\n"
                        "DEBUG %s:%d: cache hit rate 0.75\n",
                        __FILE__, line, __FILE__, line + 1));
    CHECK_EQ_MEM(got.data, got.len, want.data, want.len);
    str_free(&got);
    str_free(&want);
}

TEST(level_filters_without_evaluating) {
    FILE *f = capture();
    REQUIRE(f != nullptr);
    int calls = 0;
    log_set_level(LOG_WARN);
    log_info("dropped %d", ++calls);
    log_debug("dropped %d", ++calls);
    log_warn("kept %d", ++calls);
    log_error("kept %d", ++calls);
    CHECK_EQ_INT(calls, 2);

    Str got = {};
    REQUIRE(release(f, &got));
    CHECK(sv_find(str_view(&got), SV("WARN  ")) != SV_NPOS);
    CHECK(sv_find(str_view(&got), SV("kept 1")) != SV_NPOS);
    CHECK(sv_find(str_view(&got), SV("ERROR ")) != SV_NPOS);
    CHECK(sv_find(str_view(&got), SV("dropped")) == SV_NPOS);
    str_free(&got);
}

TEST(colors_the_level) {
    FILE *f = capture();
    REQUIRE(f != nullptr);
    setenv("CLICOLOR_FORCE", "1", 1);
    log_set_output(f); /* colour is decided again after a new output */
    log_warn("careful");

    Str got = {};
    REQUIRE(release(f, &got));
    CHECK(sv_starts_with(str_view(&got), SV(LOG_YELLOW "WARN " LOG_RESET " ")));
    CHECK(sv_ends_with(str_view(&got), SV(": careful\n")));
    str_free(&got);
    clear_color_env();
}

TEST(long_message) {
    FILE *f = capture();
    REQUIRE(f != nullptr);
    char big[10000];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    log_error("%s", big);

    Str got = {};
    REQUIRE(release(f, &got));
    StrView v = str_view(&got);
    CHECK(sv_ends_with(v, SV("x\n")));
    CHECK(got.len > sizeof big);
    str_free(&got);
}

TEST(out_of_memory_still_writes) {
    FILE *f = capture();
    REQUIRE(f != nullptr);
    test_fail_alloc_after(0);
    log_error("needs %s", "memory");
    test_fail_alloc_after(-1);

    Str got = {};
    REQUIRE(release(f, &got));
    CHECK(sv_starts_with(str_view(&got), SV("ERROR ")));
    CHECK(sv_find(str_view(&got), SV("out of memory")) != SV_NPOS);
    str_free(&got);
}

/* --- Fatal --------------------------------------------------------------- */

/* Run `fn` in a child process that logs to a pipe. True when the child died
 * of SIGABRT. What it logged lands in `out`. */
static bool aborts(void (*fn)(void), Str *out) {
    int fds[2];
    if (pipe(fds) != 0) return false;
    fflush(stdout); /* or the child's copy of the buffer prints twice */
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        close(fds[0]);
        log_set_output(fdopen(fds[1], "w"));
        fn();
        _exit(0);
    }
    close(fds[1]);
    FILE *r = fdopen(fds[0], "r");
    bool ok = r != nullptr && str_append_file(out, r);
    if (r != nullptr) fclose(r);
    int status = 0;
    waitpid(pid, &status, 0);
    return ok && WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

static void call_fatal(void) {
    log_set_level(LOG_FATAL);
    log_fatal("cannot continue: %d", 7);
}

static void call_failing_assert(void) {
    volatile int answer = 41;
    log_assert(answer == 42);
}

static void call_passing_assert(void) {
    log_assert(1 + 1 == 2);
}

TEST(fatal_writes_then_aborts) {
    clear_color_env();
    Str got = {};
    CHECK(aborts(call_fatal, &got));
    CHECK(sv_starts_with(str_view(&got), SV("FATAL ")));
    CHECK(sv_ends_with(str_view(&got), SV(": cannot continue: 7\n")));
    str_free(&got);
}

TEST(assert_aborts_only_when_false) {
    clear_color_env();
    Str got = {};
    CHECK(aborts(call_failing_assert, &got));
    CHECK(
        sv_ends_with(str_view(&got), SV(": assertion failed: answer == 42\n")));
    str_clear(&got);
    CHECK(!aborts(call_passing_assert, &got));
    CHECK_EQ_INT(got.len, 0);
    str_free(&got);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(color_off_for_files_and_pipes);
    RUN(color_on_for_terminals);
    RUN(force_turns_color_on);
    RUN(no_color_wins);
    RUN(writes_level_location_message);
    RUN(level_filters_without_evaluating);
    RUN(colors_the_level);
    RUN(long_message);
    RUN(out_of_memory_still_writes);
    RUN(fatal_writes_then_aborts);
    RUN(assert_aborts_only_when_false);
    return TEST_REPORT();
}
