/* Unit test cho log: ghi dòng log có timestamp và đọc lại n dòng cuối. */
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "minitest.h"

static void test_log_sources_and_init(void) {
    logger_t lg;
    char long_path[LOG_MAX_PATH + 10];

    ASSERT_STR_EQ("AUTH", log_source_str(LOG_SRC_AUTH));
    ASSERT_STR_EQ("CMD", log_source_str(LOG_SRC_CMD));
    ASSERT_STR_EQ("AUTO", log_source_str(LOG_SRC_AUTO));
    ASSERT_STR_EQ("SYS", log_source_str(LOG_SRC_SYS));
    ASSERT_STR_EQ("WARN", log_source_str(LOG_SRC_WARN));
    ASSERT_STR_EQ("SYS", log_source_str((log_source_t)99));

    ASSERT_EQ_INT(-1, log_init(NULL, "a.log"));
    ASSERT_EQ_INT(-1, log_init(&lg, NULL));
    ASSERT_EQ_INT(-1, log_init(&lg, ""));

    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';
    ASSERT_EQ_INT(-1, log_init(&lg, long_path));

    ASSERT_EQ_INT(0, log_init(&lg, "test_device.log.tmp"));
    ASSERT_STR_EQ("test_device.log.tmp", lg.path);
}

static void test_log_write_and_tail(void) {
    const char *path = "test_device.log.tmp";
    logger_t lg;
    char lines[5][LOG_MAX_LINE];
    int64_t ts = 1794722400LL; /* 2026-11-15 06:00:00 UTC */
    int n;

    remove(path);
    ASSERT_EQ_INT(0, log_init(&lg, path));

    /* File chưa tồn tại -> trả về 0 dòng */
    ASSERT_EQ_INT(0, log_tail(&lg, lines, 5));

    ASSERT_EQ_INT(0, log_write(&lg, ts, LOG_SRC_SYS, "started id=%s", "P01"));
    ASSERT_EQ_INT(0, log_write(&lg, ts + 60, LOG_SRC_AUTH, "connect app=9f3a1c0b"));
    ASSERT_EQ_INT(0, log_write(&lg, ts + 120, LOG_SRC_CMD, "app=9f3a1c0b PUMP 2.5L"));

    n = log_tail(&lg, lines, 5);
    ASSERT_EQ_INT(3, n);
    ASSERT_STR_EQ("2026-11-15 06:00:00 SYS started id=P01", lines[0]);
    ASSERT_STR_EQ("2026-11-15 06:01:00 AUTH connect app=9f3a1c0b", lines[1]);
    ASSERT_STR_EQ("2026-11-15 06:02:00 CMD app=9f3a1c0b PUMP 2.5L", lines[2]);

    /* Ghi thêm để vượt quá kích thước cửa sổ tail (kiểm tra xoay vòng đệm) */
    ASSERT_EQ_INT(0, log_write(&lg, ts + 180, LOG_SRC_AUTO, "schedule watered 2.0L"));
    ASSERT_EQ_INT(0, log_write(&lg, ts + 240, LOG_SRC_WARN, "sensor unreachable"));

    n = log_tail(&lg, lines, 3);
    ASSERT_EQ_INT(3, n);
    ASSERT_STR_EQ("2026-11-15 06:02:00 CMD app=9f3a1c0b PUMP 2.5L", lines[0]);
    ASSERT_STR_EQ("2026-11-15 06:03:00 AUTO schedule watered 2.0L", lines[1]);
    ASSERT_STR_EQ("2026-11-15 06:04:00 WARN sensor unreachable", lines[2]);

    remove(path);

    /* Kiểm tra các tham số lỗi */
    ASSERT_EQ_INT(-1, log_write(NULL, ts, LOG_SRC_SYS, "x"));
    ASSERT_EQ_INT(-1, log_write(&lg, ts, LOG_SRC_SYS, NULL));
    ASSERT_EQ_INT(-1, log_tail(NULL, lines, 3));
    ASSERT_EQ_INT(-1, log_tail(&lg, NULL, 3));
    ASSERT_EQ_INT(-1, log_tail(&lg, lines, 0));
}

int main(void) {
    printf("test_log\n");
    RUN_TEST(test_log_sources_and_init);
    RUN_TEST(test_log_write_and_tail);
    return MT_SUMMARY();
}
