/*
 * minitest.h - framework test tối giản, chỉ gồm một header, không phụ thuộc thư viện ngoài.
 *
 * Cách dùng trong một file tests/unit/test_xxx.c:
 *
 *     #include "minitest.h"
 *
 *     static void test_cong(void) {
 *         ASSERT_EQ_INT(4, 2 + 2);
 *     }
 *
 *     int main(void) {
 *         RUN_TEST(test_cong);
 *         return MT_SUMMARY();      // 0 nếu mọi test đạt, 1 nếu có test hỏng
 *     }
 *
 * Quy ước: các macro ASSERT_* khi sai sẽ ghi lỗi rồi `return;` khỏi hàm test,
 * nên chỉ dùng trực tiếp trong hàm trả về void (hàm test hoặc hàm helper void).
 */
#ifndef MINITEST_H
#define MINITEST_H

#include <stdio.h>
#include <string.h>

static int mt_tests_run = 0;
static int mt_tests_failed = 0;
static int mt_checks = 0;
static int mt_cur_failed = 0;

#define MT_FAIL(...)                                          \
    do {                                                      \
        mt_cur_failed = 1;                                    \
        printf("      FAIL %s:%d: ", __FILE__, __LINE__);     \
        printf(__VA_ARGS__);                                  \
        printf("\n");                                         \
    } while (0)

#define ASSERT_TRUE(cond)                                     \
    do {                                                      \
        mt_checks++;                                          \
        if (!(cond)) {                                        \
            MT_FAIL("mong đợi đúng: %s", #cond);              \
            return;                                           \
        }                                                     \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ_INT(expected, actual)                       \
    do {                                                      \
        long long mt_e = (long long)(expected);               \
        long long mt_a = (long long)(actual);                 \
        mt_checks++;                                          \
        if (mt_e != mt_a) {                                   \
            MT_FAIL("%s: mong đợi %lld, thực tế %lld",        \
                    #actual, mt_e, mt_a);                     \
            return;                                           \
        }                                                     \
    } while (0)

#define ASSERT_STR_EQ(expected, actual)                       \
    do {                                                      \
        const char *mt_e = (expected);                        \
        const char *mt_a = (actual);                          \
        mt_checks++;                                          \
        if (mt_e == NULL || mt_a == NULL ||                   \
            strcmp(mt_e, mt_a) != 0) {                        \
            MT_FAIL("%s: mong đợi \"%s\", thực tế \"%s\"",    \
                    #actual, mt_e ? mt_e : "(null)",          \
                    mt_a ? mt_a : "(null)");                  \
            return;                                           \
        }                                                     \
    } while (0)

#define ASSERT_NULL(ptr)     ASSERT_TRUE((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT_TRUE((ptr) != NULL)

/* So sánh số thực với sai số eps. */
#define ASSERT_NEAR(expected, actual, eps)                    \
    do {                                                      \
        double mt_e = (double)(expected);                     \
        double mt_a = (double)(actual);                       \
        double mt_d = mt_e - mt_a;                            \
        if (mt_d < 0) mt_d = -mt_d;                           \
        mt_checks++;                                          \
        if (!(mt_d <= (double)(eps))) {                       \
            MT_FAIL("%s: mong đợi %g, thực tế %g (eps %g)",   \
                    #actual, mt_e, mt_a, (double)(eps));      \
            return;                                           \
        }                                                     \
    } while (0)

#define RUN_TEST(fn)                                          \
    do {                                                      \
        mt_cur_failed = 0;                                    \
        mt_tests_run++;                                       \
        fn();                                                 \
        if (mt_cur_failed) {                                  \
            mt_tests_failed++;                                \
            printf("  [ FAIL ] %s\n", #fn);                   \
        } else {                                              \
            printf("  [  OK  ] %s\n", #fn);                   \
        }                                                     \
    } while (0)

/* In tổng kết và trả mã thoát cho main(). */
#define MT_SUMMARY()                                          \
    (printf("  -> %d test, %d lỗi, %d phép kiểm\n",           \
            mt_tests_run, mt_tests_failed, mt_checks),        \
     mt_tests_failed ? 1 : 0)

#endif /* MINITEST_H */
