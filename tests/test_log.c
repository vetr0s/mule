/* Tests for src/log/log.c.
 *
 * Bracketed names in comment pairs mark regions that src/log/log.h pulls into
 * the Doxygen reference with @snippet. See tests/test_str.c for the rules.
 */
#define _XOPEN_SOURCE 700 /* posix_openpt, for a real terminal to test on */

#include "test.h"

#include "log/log.c"

#include <fcntl.h>

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

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(color_off_for_files_and_pipes);
    RUN(color_on_for_terminals);
    RUN(force_turns_color_on);
    RUN(no_color_wins);
    return TEST_REPORT();
}
