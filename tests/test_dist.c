/* Tests for dist/mule.h, the single-header distribution.
 *
 * This file includes the library the way a project would, so it fails when
 * the generated header is missing a module or orders them wrongly.
 */
#define MULE_IMPLEMENTATION
#define MULE_TEST
#include "mule.h"

typedef Arr(int) IntArr;

TEST(every_module_works) {
    Arena arena = { 0 };
    Str s = { .alloc = arena_allocator(&arena) };
    CHECK(str_appendf(&s, "%d,%d", 4, 2));
    int64_t n = 0;
    CHECK(sv_parse_i64(sv_slice(str_view(&s), 0, 1), &n) && n == 4);
    uint64_t u = 0;
    CHECK(sv_parse_u64(SV("ff"), 16, &u) && u == 255);
    ArenaMark mark = arena_mark(&arena);
    CHECK(arena_alloc(&arena, 8, 1) != NULL);
    arena_restore(&arena, mark);
    arena_free(&arena);

    IntArr xs = { 0 };
    CHECK(arr_push(&xs, 7));
    CHECK_EQ_INT(arr_pop(&xs), 7);
    arr_free(&xs);

    Err e = { 0 };
    err_set(&e, ERR_INVALID, 2, "bad %s", "flag");
    CHECK(e.kind == ERR_INVALID && strcmp(e.msg, "bad flag") == 0);

    CHECK(fs_exists("tests/test_dist.c"));
    CHECK(str_eq(fs_extension(SV("a/b.c")), SV(".c")));

    Args args = { 0 };
    char *argv[] = { NULL, NULL };
    CHECK(args_parse(&args, 1, argv, NULL) && args.rest.len == 0);
    args_free(&args);

    Cmd cmd = { 0 };
    Err err = { 0 };
    CHECK(cmd_append(&cmd, "true") && cmd_run(&cmd, &err));
    arr_free(&cmd);

    Rand r = { 1 };
    CHECK(rand_below(&r, 6) < 6);
    CHECK(time_now_ns() > 0);
    log_set_level(LOG_WARN);
    log_info("below the level, so not printed");
    log_set_level(LOG_INFO);
}

TEST(version_string_matches_numbers) {
    char want[32];
    snprintf(want, sizeof want, "%d.%d.%d", MULE_VERSION_MAJOR,
             MULE_VERSION_MINOR, MULE_VERSION_PATCH);
    CHECK_EQ_STR(MULE_VERSION, want);
}

TEST(hooks_reach_the_library) {
    Str s = { 0 };
    test_fail_alloc_after(0);
    CHECK(!str_append(&s, SV("x")));
    test_fail_alloc_after(-1);
    CHECK(s.data == NULL);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(every_module_works);
    RUN(hooks_reach_the_library);
    RUN(version_string_matches_numbers);
    return TEST_REPORT();
}
