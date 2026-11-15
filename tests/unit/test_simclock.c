/* Unit test cho simclock: đồng hồ giả lập có hệ số tăng tốc. */
#include "minitest.h"
#include "simclock.h"

static void test_init_and_advance(void) {
    simclock_t clk;
    int64_t t0 = 1700000000000LL; /* ms */

    ASSERT_EQ_INT(-1, simclock_init(NULL, t0, 0, 1.0));
    ASSERT_EQ_INT(-1, simclock_init(&clk, t0, 0, 0.0));
    ASSERT_EQ_INT(-1, simclock_init(&clk, t0, 0, -5.0));
    ASSERT_EQ_INT(-1, simclock_init(&clk, -1, 0, 1.0));

    /* Mặc định sim_start_ms <= 0 sẽ lấy bằng real_now_ms */
    ASSERT_EQ_INT(0, simclock_init(&clk, t0, 0, 60.0));
    ASSERT_EQ_INT(t0, simclock_now_ms(&clk, t0));
    ASSERT_EQ_INT(t0 / 1000, simclock_now_sec(&clk, t0));

    /* Sau 10 giây thật với hệ số 60x -> tăng 600 giây giả lập */
    ASSERT_EQ_INT(t0 + 600000LL, simclock_now_ms(&clk, t0 + 10000LL));
    ASSERT_EQ_INT(t0 / 1000 + 600, simclock_now_sec(&clk, t0 + 10000LL));

    /* Nếu thời gian truyền vào nhỏ hơn mốc bắt đầu -> không lùi âm */
    ASSERT_EQ_INT(t0, simclock_now_ms(&clk, t0 - 5000LL));
    ASSERT_EQ_INT(0, simclock_now_ms(NULL, t0));

    /* Khởi tạo với sim_start_ms riêng */
    ASSERT_EQ_INT(0, simclock_init(&clk, 1000, 50000, 10.0));
    ASSERT_EQ_INT(50000 + 20000, simclock_now_ms(&clk, 3000));
}

static void test_to_real_ms(void) {
    simclock_t clk;

    simclock_init(&clk, 1000, 1000, 60.0);
    ASSERT_EQ_INT(1000, simclock_to_real_ms(&clk, 60000));
    ASSERT_EQ_INT(1, simclock_to_real_ms(&clk, 10));
    ASSERT_EQ_INT(0, simclock_to_real_ms(&clk, 0));
    ASSERT_EQ_INT(0, simclock_to_real_ms(&clk, -100));
    ASSERT_EQ_INT(0, simclock_to_real_ms(NULL, 1000));
}

static void test_minute_of_day_and_format(void) {
    char buf[32];
    /* 2026-11-15 06:30:45 UTC = 1794724245 */
    int64_t ts = 1794724245LL;

    ASSERT_EQ_INT(6 * 60 + 30, simclock_minute_of_day(ts));
    ASSERT_EQ_INT(0, simclock_minute_of_day(0));
    ASSERT_EQ_INT(-1, simclock_minute_of_day(-1));

    ASSERT_EQ_INT(0, simclock_format_time(0, buf, sizeof buf));
    ASSERT_STR_EQ("1970-01-01 00:00:00", buf);

    ASSERT_EQ_INT(0, simclock_format_time(ts, buf, sizeof buf));
    ASSERT_STR_EQ("2026-11-15 06:30:45", buf);

    ASSERT_EQ_INT(-1, simclock_format_time(-1, buf, sizeof buf));
    ASSERT_EQ_INT(-1, simclock_format_time(ts, NULL, sizeof buf));
    ASSERT_EQ_INT(-1, simclock_format_time(ts, buf, 10));
}

static void test_real_ms(void) {
    int64_t r1 = simclock_real_ms();
    int64_t r2 = simclock_real_ms();
    ASSERT_TRUE(r1 > 1700000000000LL);
    ASSERT_TRUE(r2 >= r1);
}

int main(void) {
    printf("test_simclock\n");
    RUN_TEST(test_init_and_advance);
    RUN_TEST(test_to_real_ms);
    RUN_TEST(test_minute_of_day_and_format);
    RUN_TEST(test_real_ms);
    return MT_SUMMARY();
}
