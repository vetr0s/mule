/* test: unit tests for your own code, with leak checks on every test. */
#define MULE_IMPLEMENTATION
#define MULE_TEST
#include "mule.h"

/* The code under test: wrap `text` in brackets. */
static bool bracket(Str *out, StrView text) {
    return str_push(out, '[') && str_append(out, text) && str_push(out, ']');
}

TEST(brackets_text) {
    Str s = { 0 };
    REQUIRE(bracket(&s, SV("mule")));
    CHECK_EQ_STR(str_cstr(&s), "[mule]");
    str_free(&s);
}

TEST(brackets_nothing) {
    Str s = { 0 };
    REQUIRE(bracket(&s, SV("")));
    CHECK_EQ_INT(s.len, 2);
    str_free(&s);
}

TEST(out_of_memory_is_reported) {
    Str s = { 0 };
    test_fail_alloc_after(0);
    CHECK(!bracket(&s, SV("mule")));
    str_free(&s);
}

int main(int argc, char **argv) {
    test_init(argc, argv);
    RUN(brackets_text);
    RUN(brackets_nothing);
    RUN(out_of_memory_is_reported);
    return TEST_REPORT();
}
