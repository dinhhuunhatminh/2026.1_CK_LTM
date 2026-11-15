/* Unit test cho util: sinh hex ngẫu nhiên, xử lý chuỗi, parse số và giờ HH:MM. */
#include <stdio.h>
#include <string.h>

#include "minitest.h"
#include "util.h"

static void test_random_hex(void) {
    char hex1[17];
    char hex2[17];
    const char *tmp_file = "test_rand_seed.tmp";
    FILE *fp;

    ASSERT_EQ_INT(0, util_random_hex(hex1, 16));
    ASSERT_EQ_INT(0, util_random_hex(hex2, 16));
    ASSERT_EQ_INT(1, util_valid_hex(hex1, 16));
    ASSERT_EQ_INT(1, util_valid_hex(hex2, 16));
    ASSERT_TRUE(strcmp(hex1, hex2) != 0);

    /* Kiểm thử đọc từ file giả lập xác định */
    fp = fopen(tmp_file, "wb");
    ASSERT_NOT_NULL(fp);
    fwrite("\x01\x23\x45\xab\xcd\xef", 1, 6, fp);
    fclose(fp);

    ASSERT_EQ_INT(0, util_random_hex_from_file(tmp_file, hex1, 12));
    ASSERT_STR_EQ("012345abcdef", hex1);
    ASSERT_EQ_INT(0, util_random_hex_from_file(tmp_file, hex1, 5));
    ASSERT_STR_EQ("01234", hex1);
    /* File ngắn hơn số byte cần thiết -> lỗi */
    ASSERT_EQ_INT(-1, util_random_hex_from_file(tmp_file, hex1, 16));
    remove(tmp_file);

    ASSERT_EQ_INT(-1, util_random_hex_from_file("khong_ton_tai.tmp", hex1, 8));
    ASSERT_EQ_INT(-1, util_random_hex_from_file(NULL, hex1, 8));
    ASSERT_EQ_INT(-1, util_random_hex(NULL, 16));
    ASSERT_EQ_INT(-1, util_random_hex(hex1, 0));
    ASSERT_EQ_INT(-1, util_random_hex(hex1, 200));
}

static void test_trim_and_str_copy(void) {
    char buf[32];

    ASSERT_NULL(util_trim(NULL));

    strcpy(buf, "   hello world \t\r\n");
    ASSERT_STR_EQ("hello world", util_trim(buf));

    strcpy(buf, "no_spaces");
    ASSERT_STR_EQ("no_spaces", util_trim(buf));

    strcpy(buf, "   \t\r\n  ");
    ASSERT_STR_EQ("", util_trim(buf));

    ASSERT_EQ_INT(0, util_str_copy(buf, sizeof buf, "garden"));
    ASSERT_STR_EQ("garden", buf);

    ASSERT_EQ_INT(-1, util_str_copy(buf, 4, "garden"));
    ASSERT_STR_EQ("gar", buf);

    ASSERT_EQ_INT(-1, util_str_copy(NULL, 10, "a"));
    ASSERT_EQ_INT(-1, util_str_copy(buf, 0, "a"));
    ASSERT_EQ_INT(-1, util_str_copy(buf, 10, NULL));
}

static void test_valid_id_and_hex(void) {
    ASSERT_EQ_INT(1, util_valid_id("P01", 16));
    ASSERT_EQ_INT(1, util_valid_id("sensor_A-1", 16));
    ASSERT_EQ_INT(0, util_valid_id(NULL, 16));
    ASSERT_EQ_INT(0, util_valid_id("", 16));
    ASSERT_EQ_INT(0, util_valid_id("P01", 0));
    ASSERT_EQ_INT(0, util_valid_id("too_long_id", 5));
    ASSERT_EQ_INT(0, util_valid_id("bad space", 16));
    ASSERT_EQ_INT(0, util_valid_id("bad$char", 16));

    ASSERT_EQ_INT(1, util_valid_hex("9f3a1c0b7d2e4a58", 16));
    ASSERT_EQ_INT(1, util_valid_hex("ABCDEF01", 8));
    ASSERT_EQ_INT(0, util_valid_hex(NULL, 8));
    ASSERT_EQ_INT(0, util_valid_hex("1234", 0));
    ASSERT_EQ_INT(0, util_valid_hex("123", 4));
    ASSERT_EQ_INT(0, util_valid_hex("12345", 4));
    ASSERT_EQ_INT(0, util_valid_hex("123g", 4));
}

static void test_parse_int_and_double(void) {
    int iv = 0;
    double dv = 0.0;

    ASSERT_EQ_INT(0, util_parse_int("42", 0, 100, &iv));
    ASSERT_EQ_INT(42, iv);
    ASSERT_EQ_INT(0, util_parse_int("-10", -20, 20, &iv));
    ASSERT_EQ_INT(-10, iv);

    ASSERT_EQ_INT(-1, util_parse_int(NULL, 0, 10, &iv));
    ASSERT_EQ_INT(-1, util_parse_int("5", 0, 10, NULL));
    ASSERT_EQ_INT(-1, util_parse_int("5", 10, 0, &iv));
    ASSERT_EQ_INT(-1, util_parse_int("", 0, 10, &iv));
    ASSERT_EQ_INT(-1, util_parse_int(" 5", 0, 10, &iv));
    ASSERT_EQ_INT(-1, util_parse_int("5abc", 0, 10, &iv));
    ASSERT_EQ_INT(-1, util_parse_int("11", 0, 10, &iv));
    ASSERT_EQ_INT(-1, util_parse_int("-1", 0, 10, &iv));

    ASSERT_EQ_INT(0, util_parse_double("12.5", 0.0, 100.0, &dv));
    ASSERT_NEAR(12.5, dv, 1e-9);
    ASSERT_EQ_INT(0, util_parse_double("0", 0.0, 100.0, &dv));
    ASSERT_NEAR(0.0, dv, 1e-9);

    ASSERT_EQ_INT(-1, util_parse_double(NULL, 0.0, 10.0, &dv));
    ASSERT_EQ_INT(-1, util_parse_double("1.0", 0.0, 10.0, NULL));
    ASSERT_EQ_INT(-1, util_parse_double("1.0", 10.0, 0.0, &dv));
    ASSERT_EQ_INT(-1, util_parse_double("", 0.0, 10.0, &dv));
    ASSERT_EQ_INT(-1, util_parse_double(" 1.0", 0.0, 10.0, &dv));
    ASSERT_EQ_INT(-1, util_parse_double("1.0x", 0.0, 10.0, &dv));
    ASSERT_EQ_INT(-1, util_parse_double("10.1", 0.0, 10.0, &dv));
    ASSERT_EQ_INT(-1, util_parse_double("-0.1", 0.0, 10.0, &dv));
    ASSERT_EQ_INT(-1, util_parse_double("nan", -10.0, 10.0, &dv));
    ASSERT_EQ_INT(-1, util_parse_double("inf", -10.0, 10.0, &dv));
}

static void test_hhmm(void) {
    int mins = -1;
    char buf[16];

    ASSERT_EQ_INT(0, util_parse_hhmm("00:00", &mins));
    ASSERT_EQ_INT(0, mins);
    ASSERT_EQ_INT(0, util_parse_hhmm("06:30", &mins));
    ASSERT_EQ_INT(390, mins);
    ASSERT_EQ_INT(0, util_parse_hhmm("23:59", &mins));
    ASSERT_EQ_INT(1439, mins);

    ASSERT_EQ_INT(0, util_format_hhmm(390, buf, sizeof buf));
    ASSERT_STR_EQ("06:30", buf);

    ASSERT_EQ_INT(-1, util_parse_hhmm(NULL, &mins));
    ASSERT_EQ_INT(-1, util_parse_hhmm("06:00", NULL));
    ASSERT_EQ_INT(-1, util_parse_hhmm("6:00", &mins));
    ASSERT_EQ_INT(-1, util_parse_hhmm("06-00", &mins));
    ASSERT_EQ_INT(-1, util_parse_hhmm("24:00", &mins));
    ASSERT_EQ_INT(-1, util_parse_hhmm("12:60", &mins));
    ASSERT_EQ_INT(-1, util_parse_hhmm("ab:cd", &mins));

    ASSERT_EQ_INT(-1, util_format_hhmm(-1, buf, sizeof buf));
    ASSERT_EQ_INT(-1, util_format_hhmm(1440, buf, sizeof buf));
    ASSERT_EQ_INT(-1, util_format_hhmm(60, NULL, 10));
    ASSERT_EQ_INT(-1, util_format_hhmm(60, buf, 5));
}

int main(void) {
    printf("test_util\n");
    RUN_TEST(test_random_hex);
    RUN_TEST(test_trim_and_str_copy);
    RUN_TEST(test_valid_id_and_hex);
    RUN_TEST(test_parse_int_and_double);
    RUN_TEST(test_hhmm);
    return MT_SUMMARY();
}
