#include "test.h"
#include "str.c"

TEST(zero_init_is_empty) {
    Str s = {};
    CHECK_EQ_INT(s.len, 0);
    CHECK_EQ_STR(str_cstr(&s), "");
    str_clear(&s);
    str_free(&s);
}

TEST(from_and_append) {
    Str s = {};
    CHECK(str_from_cstr(&s, "foo"));
    CHECK(str_append(&s, SV("bar")));
    CHECK(str_push(&s, '!'));
    CHECK_EQ_STR(str_cstr(&s), "foobar!");
    CHECK_EQ_INT(s.len, 7);
    CHECK(s.capacity >= s.len);
    str_free(&s);
    CHECK(s.data == nullptr);
}

TEST(appendf) {
    Str s = {};
    CHECK(str_appendf(&s, "%d-%s", 42, "x"));
    CHECK(str_appendf(&s, "|%05.1f", 3.14159));
    CHECK_EQ_STR(str_cstr(&s), "42-x|003.1");
    str_free(&s);
}

TEST(grows_past_initial_capacity) {
    Str s = {};
    for (int i = 0; i < 1000; i++) CHECK(str_push(&s, 'a'));
    CHECK_EQ_INT(s.len, 1000);
    CHECK(s.data[1000] == '\0');
    CHECK(str_shrink_to_fit(&s));
    CHECK_EQ_INT(s.capacity, 1000);
    str_free(&s);
}

TEST(self_append) {
    Str s = {};
    CHECK(str_from_cstr(&s, "abc"));
    for (int i = 0; i < 6; i++) CHECK(str_append(&s, str_view(&s)));
    CHECK_EQ_INT(s.len, 3 * 64);
    CHECK(str_eq((StrView){ s.data + 189, 3 }, SV("abc")));
    str_free(&s);
}

TEST(clear_keeps_capacity) {
    Str s = {};
    CHECK(str_with_capacity(&s, 100));
    CHECK(s.capacity >= 100);
    CHECK(str_append_cstr(&s, "data"));
    str_clear(&s);
    CHECK_EQ_INT(s.len, 0);
    CHECK_EQ_STR(str_cstr(&s), "");
    CHECK(s.capacity >= 100);
    str_free(&s);
}

TEST(equality) {
    CHECK(str_eq(SV("abc"), sv_from_cstr("abc")));
    CHECK(!str_eq(SV("abc"), SV("abd")));
    CHECK(!str_eq(SV("abc"), SV("ab")));
    CHECK(str_eq(SV(""), sv_from_cstr(nullptr)));
}

int main(void) {
    RUN(zero_init_is_empty);
    RUN(from_and_append);
    RUN(appendf);
    RUN(grows_past_initial_capacity);
    RUN(self_append);
    RUN(clear_keeps_capacity);
    RUN(equality);
    return TEST_REPORT();
}
