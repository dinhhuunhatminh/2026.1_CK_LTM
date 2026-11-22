/* Unit test cho options: phân tích tham số dòng lệnh của device_sim. */
#include <stdio.h>
#include <string.h>

#include "minitest.h"
#include "options.h"
#include "server.h"

/* Gọi options_parse với danh sách đối số cho trước (argv[0] tự thêm). */
static int parse(dev_options_t *o, char *err, const char *a1, const char *a2, const char *a3,
                 const char *a4, const char *a5, const char *a6, const char *a7, const char *a8) {
    char *argv[12];
    int argc = 0;
    const char *args[] = {a1, a2, a3, a4, a5, a6, a7, a8};
    argv[argc++] = (char *)"device_sim";
    for (size_t i = 0; i < sizeof args / sizeof args[0]; i++) {
        if (args[i]) {
            argv[argc++] = (char *)args[i];
        }
    }
    err[0] = '\0';
    return options_parse(argc, argv, o, err, 200);
}

static void test_minimal_options_and_defaults(void) {
    dev_options_t o; char err[200];
    ASSERT_EQ_INT(0, parse(&o, err, "--type", "pump", "--id", "P01", "--port", "7002", NULL, NULL));
    ASSERT_EQ_INT(DEV_PUMP, o.cfg.type);
    ASSERT_STR_EQ("P01", o.cfg.id);
    ASSERT_EQ_INT(7002, o.port);
    ASSERT_EQ_INT(SERVER_SCAN_PORT, o.scan_port);
    ASSERT_NEAR(1.0, o.cfg.timescale, 1e-9);
    ASSERT_STR_EQ("data", o.cfg.state_dir);
    ASSERT_STR_EQ("logs", o.cfg.log_dir);
    ASSERT_STR_EQ("0.0.0.0", o.bind_ip);
    ASSERT_STR_EQ("", o.cfg.password);
}

static void test_all_options(void) {
    dev_options_t o; char err[200];
    char *argv[] = {(char *)"device_sim", (char *)"--type", (char *)"LIGHT", (char *)"--id",
                    (char *)"L-01", (char *)"--port", (char *)"7004", (char *)"--password",
                    (char *)"abcd", (char *)"--timescale", (char *)"60", (char *)"--state-dir",
                    (char *)"/tmp/s", (char *)"--log-dir", (char *)"/tmp/l", (char *)"--bind",
                    (char *)"127.0.0.1", (char *)"--scan-port", (char *)"19999"};
    ASSERT_EQ_INT(0, options_parse((int)(sizeof argv / sizeof argv[0]), argv, &o, err, sizeof err));
    ASSERT_EQ_INT(DEV_LIGHT, o.cfg.type);
    ASSERT_STR_EQ("L-01", o.cfg.id);
    ASSERT_EQ_INT(7004, o.port);
    ASSERT_STR_EQ("abcd", o.cfg.password);
    ASSERT_NEAR(60.0, o.cfg.timescale, 1e-9);
    ASSERT_STR_EQ("/tmp/s", o.cfg.state_dir);
    ASSERT_STR_EQ("/tmp/l", o.cfg.log_dir);
    ASSERT_STR_EQ("127.0.0.1", o.bind_ip);
    ASSERT_EQ_INT(19999, o.scan_port);
}

static void test_type_is_case_insensitive(void) {
    dev_options_t o; char err[200];
    ASSERT_EQ_INT(0, parse(&o, err, "--type", "Sensor", "--id", "S1", "--port", "1", NULL, NULL));
    ASSERT_EQ_INT(DEV_SENSOR, o.cfg.type);
    ASSERT_EQ_INT(0, parse(&o, err, "--type", "fert", "--id", "F1", "--port", "65535", NULL, NULL));
    ASSERT_EQ_INT(DEV_FERT, o.cfg.type);
}

static void test_help(void) {
    dev_options_t o; char err[200];
    ASSERT_EQ_INT(1, parse(&o, err, "--help", NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_EQ_INT(1, parse(&o, err, "--type", "pump", "-h", NULL, NULL, NULL, NULL, NULL));
}

static void test_missing_required(void) {
    dev_options_t o; char err[200];
    ASSERT_EQ_INT(-1, parse(&o, err, "--id", "P01", "--port", "7002", NULL, NULL, NULL, NULL));
    ASSERT_TRUE(strstr(err, "--type") != NULL);
    ASSERT_EQ_INT(-1, parse(&o, err, "--type", "pump", "--port", "7002", NULL, NULL, NULL, NULL));
    ASSERT_TRUE(strstr(err, "--id") != NULL);
    ASSERT_EQ_INT(-1, parse(&o, err, "--type", "pump", "--id", "P01", NULL, NULL, NULL, NULL));
    ASSERT_TRUE(strstr(err, "--port") != NULL);
    ASSERT_EQ_INT(-1, parse(&o, err, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
}

static void test_invalid_values(void) {
    dev_options_t o; char err[200];
    ASSERT_EQ_INT(-1, parse(&o, err, "--type", "robot", NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_TRUE(strstr(err, "robot") != NULL);
    ASSERT_EQ_INT(-1, parse(&o, err, "--type", "mot_cai_ten_rat_dai_vuot_qua_gioi_han", NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_EQ_INT(-1, parse(&o, err, "--id", "id sai", NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_EQ_INT(-1, parse(&o, err, "--id", "idquadai_17_kitu_", NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_EQ_INT(-1, parse(&o, err, "--port", "0", NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_EQ_INT(-1, parse(&o, err, "--port", "70000", NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_EQ_INT(-1, parse(&o, err, "--port", "abc", NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_EQ_INT(-1, parse(&o, err, "--scan-port", "-5", NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_EQ_INT(-1, parse(&o, err, "--timescale", "0", NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_EQ_INT(-1, parse(&o, err, "--timescale", "nhanh", NULL, NULL, NULL, NULL, NULL, NULL));
}

static void test_too_long_strings(void) {
    dev_options_t o; char err[200]; char big[400];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    ASSERT_EQ_INT(-1, parse(&o, err, "--password", big, NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_EQ_INT(-1, parse(&o, err, "--state-dir", big, NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_EQ_INT(-1, parse(&o, err, "--log-dir", big, NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_EQ_INT(-1, parse(&o, err, "--bind", big, NULL, NULL, NULL, NULL, NULL, NULL));
}

static void test_unknown_option_and_missing_value(void) {
    dev_options_t o; char err[200];
    ASSERT_EQ_INT(-1, parse(&o, err, "--lung-tung", "1", NULL, NULL, NULL, NULL, NULL, NULL));
    ASSERT_TRUE(strstr(err, "--lung-tung") != NULL);
    ASSERT_EQ_INT(-1, parse(&o, err, "--type", "pump", "--id", NULL, NULL, NULL, NULL, NULL));
    ASSERT_TRUE(strstr(err, "--id") != NULL);
    ASSERT_EQ_INT(-1, options_parse(0, NULL, &o, err, sizeof err));
    ASSERT_EQ_INT(-1, options_parse(1, NULL, &o, err, sizeof err));
}

static void test_usage_text(void) {
    FILE *f = tmpfile();
    char buf[2048];
    size_t n;
    ASSERT_NOT_NULL(f);
    options_usage(f, "device_sim");
    options_usage(f, NULL);
    rewind(f);
    n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    fclose(f);
    ASSERT_TRUE(strstr(buf, "--type") != NULL);
    ASSERT_TRUE(strstr(buf, "9999") != NULL);
}

int main(void) {
    printf("test_options\n");
    RUN_TEST(test_minimal_options_and_defaults);
    RUN_TEST(test_all_options);
    RUN_TEST(test_type_is_case_insensitive);
    RUN_TEST(test_help);
    RUN_TEST(test_missing_required);
    RUN_TEST(test_invalid_values);
    RUN_TEST(test_too_long_strings);
    RUN_TEST(test_unknown_option_and_missing_value);
    RUN_TEST(test_usage_text);
    return MT_SUMMARY();
}
