/* Smoke test: kiểm tra framework minitest và toolchain build hoạt động. */
#include "minitest.h"

static void test_assert_true_false(void) {
    ASSERT_TRUE(1 == 1);
    ASSERT_FALSE(1 == 2);
}

static void test_assert_eq_int(void) {
    ASSERT_EQ_INT(4, 2 + 2);
    ASSERT_EQ_INT(-1, (long)-1);
}

static void test_assert_str_eq(void) {
    ASSERT_STR_EQ("garden", "garden");
    ASSERT_STR_EQ("", "");
}

static void test_assert_null(void) {
    const char *p = NULL;
    const char *q = "x";
    ASSERT_NULL(p);
    ASSERT_NOT_NULL(q);
}

static void test_assert_near(void) {
    ASSERT_NEAR(0.3, 0.1 + 0.2, 1e-9);
}

int main(void) {
    printf("test_minitest\n");
    RUN_TEST(test_assert_true_false);
    RUN_TEST(test_assert_eq_int);
    RUN_TEST(test_assert_str_eq);
    RUN_TEST(test_assert_null);
    RUN_TEST(test_assert_near);
    return MT_SUMMARY();
}
