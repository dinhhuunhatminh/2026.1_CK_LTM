/* Unit test cho proto: phân tích lệnh, kiểm tra loại thiết bị và dựng/đọc phản hồi. */
#include <string.h>

#include "minitest.h"
#include "proto.h"

static void test_dev_type_conversions(void) {
    ASSERT_STR_EQ("SENSOR", proto_dev_type_str(DEV_SENSOR));
    ASSERT_STR_EQ("PUMP", proto_dev_type_str(DEV_PUMP));
    ASSERT_STR_EQ("FERT", proto_dev_type_str(DEV_FERT));
    ASSERT_STR_EQ("LIGHT", proto_dev_type_str(DEV_LIGHT));
    ASSERT_STR_EQ("UNKNOWN", proto_dev_type_str(DEV_NONE));

    ASSERT_EQ_INT(DEV_SENSOR, proto_dev_type_from_str("SENSOR"));
    ASSERT_EQ_INT(DEV_PUMP, proto_dev_type_from_str("PUMP"));
    ASSERT_EQ_INT(DEV_FERT, proto_dev_type_from_str("FERT"));
    ASSERT_EQ_INT(DEV_LIGHT, proto_dev_type_from_str("LIGHT"));
    ASSERT_EQ_INT(DEV_NONE, proto_dev_type_from_str("MOTOR"));
    ASSERT_EQ_INT(DEV_NONE, proto_dev_type_from_str(NULL));
}

static void test_code_tables_and_cmd_names(void) {
    ASSERT_STR_EQ("OK", proto_code_text(PROTO_OK));
    ASSERT_STR_EQ("MULTI", proto_code_text(PROTO_MULTI));
    ASSERT_STR_EQ("BAD PASSWORD", proto_code_text(PROTO_BAD_PASSWORD));
    ASSERT_STR_EQ("BAD TOKEN", proto_code_text(PROTO_BAD_TOKEN));
    ASSERT_STR_EQ("BAD ARGUMENT", proto_code_text(PROTO_BAD_ARG));
    ASSERT_STR_EQ("UNSUPPORTED", proto_code_text(PROTO_UNSUPPORTED));
    ASSERT_STR_EQ("DEVICE OFF", proto_code_text(PROTO_DEVICE_OFF));
    ASSERT_STR_EQ("TOO MANY APPS", proto_code_text(PROTO_TOO_MANY_APPS));
    ASSERT_STR_EQ("LOCKED", proto_code_text(PROTO_LOCKED));
    ASSERT_STR_EQ("UNKNOWN COMMAND", proto_code_text(PROTO_UNKNOWN_CMD));
    ASSERT_STR_EQ("UNKNOWN ERROR", proto_code_text(PROTO_UNKNOWN_ERR));
    ASSERT_STR_EQ("UNKNOWN ERROR", proto_code_text(777));

    ASSERT_NOT_NULL(proto_code_vi(PROTO_OK));
    ASSERT_NOT_NULL(proto_code_vi(PROTO_MULTI));
    ASSERT_NOT_NULL(proto_code_vi(PROTO_BAD_PASSWORD));
    ASSERT_NOT_NULL(proto_code_vi(PROTO_BAD_TOKEN));
    ASSERT_NOT_NULL(proto_code_vi(PROTO_BAD_ARG));
    ASSERT_NOT_NULL(proto_code_vi(PROTO_UNSUPPORTED));
    ASSERT_NOT_NULL(proto_code_vi(PROTO_DEVICE_OFF));
    ASSERT_NOT_NULL(proto_code_vi(PROTO_TOO_MANY_APPS));
    ASSERT_NOT_NULL(proto_code_vi(PROTO_LOCKED));
    ASSERT_NOT_NULL(proto_code_vi(PROTO_UNKNOWN_CMD));
    ASSERT_NOT_NULL(proto_code_vi(PROTO_UNKNOWN_ERR));
    ASSERT_NOT_NULL(proto_code_vi(777));

    ASSERT_STR_EQ("CONNECT", proto_cmd_name(CMD_CONNECT));
    ASSERT_STR_EQ("INFO", proto_cmd_name(CMD_INFO));
    ASSERT_STR_EQ("STATUS", proto_cmd_name(CMD_STATUS));
    ASSERT_STR_EQ("PARAMS", proto_cmd_name(CMD_PARAMS));
    ASSERT_STR_EQ("POWER", proto_cmd_name(CMD_POWER));
    ASSERT_STR_EQ("PASSWD", proto_cmd_name(CMD_PASSWD));
    ASSERT_STR_EQ("LOG", proto_cmd_name(CMD_LOG));
    ASSERT_STR_EQ("SET", proto_cmd_name(CMD_SET));
    ASSERT_STR_EQ("DATA", proto_cmd_name(CMD_DATA));
    ASSERT_STR_EQ("HISTORY", proto_cmd_name(CMD_HISTORY));
    ASSERT_STR_EQ("FEED", proto_cmd_name(CMD_FEED));
    ASSERT_STR_EQ("PUMP", proto_cmd_name(CMD_PUMP));
    ASSERT_STR_EQ("FERTILIZE", proto_cmd_name(CMD_FERTILIZE));
    ASSERT_STR_EQ("LIGHT", proto_cmd_name(CMD_LIGHT));
    ASSERT_STR_EQ("NONE", proto_cmd_name(CMD_NONE));
}

static void test_parse_connect(void) {
    proto_cmd_t cmd;

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("CONNECT 9f3a1c0b7d2e4a58 1234\r\n", &cmd));
    ASSERT_EQ_INT(CMD_CONNECT, cmd.type);
    ASSERT_STR_EQ("", cmd.token);
    ASSERT_EQ_INT(2, cmd.argc);
    ASSERT_STR_EQ("9f3a1c0b7d2e4a58", cmd.argv[0]);
    ASSERT_STR_EQ("1234", cmd.argv[1]);

    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("CONNECT", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("CONNECT only_one", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("CONNECT app pass extra", &cmd));
}

static void test_parse_common_commands(void) {
    proto_cmd_t cmd;

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("5b1e77a09c3d4f21 INFO", &cmd));
    ASSERT_EQ_INT(CMD_INFO, cmd.type);
    ASSERT_STR_EQ("5b1e77a09c3d4f21", cmd.token);
    ASSERT_EQ_INT(0, cmd.argc);

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok STATUS", &cmd));
    ASSERT_EQ_INT(CMD_STATUS, cmd.type);

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok PARAMS", &cmd));
    ASSERT_EQ_INT(CMD_PARAMS, cmd.type);

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok POWER ON", &cmd));
    ASSERT_EQ_INT(CMD_POWER, cmd.type);
    ASSERT_STR_EQ("ON", cmd.argv[0]);

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok POWER OFF", &cmd));
    ASSERT_STR_EQ("OFF", cmd.argv[0]);
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok POWER", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok POWER AUTO", &cmd));

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok PASSWD 1234 abcd5678", &cmd));
    ASSERT_EQ_INT(CMD_PASSWD, cmd.type);
    ASSERT_EQ_INT(2, cmd.argc);
    ASSERT_STR_EQ("1234", cmd.argv[0]);
    ASSERT_STR_EQ("abcd5678", cmd.argv[1]);
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok PASSWD 1234", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok PASSWD 1 2 3", &cmd));

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok LOG", &cmd));
    ASSERT_EQ_INT(CMD_LOG, cmd.type);
    ASSERT_EQ_INT(0, cmd.argc);

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok LOG 20", &cmd));
    ASSERT_EQ_INT(1, cmd.argc);
    ASSERT_STR_EQ("20", cmd.argv[0]);
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok LOG 0", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok LOG -5", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok LOG abc", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok LOG 10 20", &cmd));
}

static void test_parse_device_specific_commands(void) {
    proto_cmd_t cmd;

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok DATA", &cmd));
    ASSERT_EQ_INT(CMD_DATA, cmd.type);
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok DATA 1", &cmd));

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok HISTORY", &cmd));
    ASSERT_EQ_INT(CMD_HISTORY, cmd.type);
    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok HISTORY 5", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok HISTORY bad", &cmd));

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok FEED WATER 2.5", &cmd));
    ASSERT_EQ_INT(CMD_FEED, cmd.type);
    ASSERT_EQ_INT(2, cmd.argc);
    ASSERT_STR_EQ("WATER", cmd.argv[0]);
    ASSERT_STR_EQ("2.5", cmd.argv[1]);

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok FEED NPK 1.5 2.0 1.0", &cmd));
    ASSERT_EQ_INT(CMD_FEED, cmd.type);
    ASSERT_EQ_INT(4, cmd.argc);
    ASSERT_STR_EQ("NPK", cmd.argv[0]);
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok FEED WATER", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok FEED NPK 1 2", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok FEED OTHER 1", &cmd));

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok PUMP 2.5", &cmd));
    ASSERT_EQ_INT(CMD_PUMP, cmd.type);
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok PUMP", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok PUMP 1 2", &cmd));

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok FERTILIZE", &cmd));
    ASSERT_EQ_INT(CMD_FERTILIZE, cmd.type);
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok FERTILIZE 1", &cmd));

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok LIGHT ON", &cmd));
    ASSERT_EQ_INT(CMD_LIGHT, cmd.type);
    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok LIGHT OFF", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok LIGHT DIM", &cmd));
}

static void test_parse_set_commands(void) {
    proto_cmd_t cmd;

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok SET INTERVAL 5", &cmd));
    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok SET HMAX 70", &cmd));
    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok SET HMIN 30", &cmd));
    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok SET SCHEDULE 06:00,17:30", &cmd));
    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok SET VOLUME 2.5", &cmd));
    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok SET NMIN 20", &cmd));
    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok SET PMIN 20", &cmd));
    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok SET KMIN 20", &cmd));
    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok SET CONC 5", &cmd));
    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok SET NPK_RATIO 1:1:1", &cmd));
    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok SET WATT 150", &cmd));

    ASSERT_EQ_INT(PROTO_OK, proto_parse_cmd("tok SET SENSOR 127.0.0.1 7001 1234", &cmd));
    ASSERT_EQ_INT(CMD_SET, cmd.type);
    ASSERT_EQ_INT(4, cmd.argc);
    ASSERT_STR_EQ("SENSOR", cmd.argv[0]);
    ASSERT_STR_EQ("127.0.0.1", cmd.argv[1]);
    ASSERT_STR_EQ("7001", cmd.argv[2]);
    ASSERT_STR_EQ("1234", cmd.argv[3]);

    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok SET", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok SET HMIN", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok SET HMIN 30 40", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok SET SENSOR 127.0.0.1 7001", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok SET UNKNOWN_KEY 10", &cmd));
}

static void test_parse_errors_and_edge_cases(void) {
    proto_cmd_t cmd;
    char long_line[PROTO_MAX_LINE + 20];
    char long_tok[PROTO_MAX_TOKEN + 20];

    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd(NULL, &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok INFO", NULL));
    ASSERT_EQ_INT(PROTO_UNKNOWN_CMD, proto_parse_cmd("", &cmd));
    ASSERT_EQ_INT(PROTO_UNKNOWN_CMD, proto_parse_cmd("   \t\r\n", &cmd));

    /* Thiếu token trước lệnh đã biết -> 201 BAD TOKEN */
    ASSERT_EQ_INT(PROTO_BAD_TOKEN, proto_parse_cmd("INFO", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_TOKEN, proto_parse_cmd("SET HMIN 30", &cmd));
    ASSERT_EQ_INT(PROTO_BAD_TOKEN, proto_parse_cmd("POWER ON", &cmd));

    /* Có 1 từ không phải lệnh, hoặc lệnh lạ sau token -> 300 UNKNOWN COMMAND */
    ASSERT_EQ_INT(PROTO_UNKNOWN_CMD, proto_parse_cmd("5b1e77a09c3d4f21", &cmd));
    ASSERT_EQ_INT(PROTO_UNKNOWN_CMD, proto_parse_cmd("5b1e77a09c3d4f21 REBOOT", &cmd));
    ASSERT_EQ_INT(PROTO_UNKNOWN_CMD, proto_parse_cmd("5b1e77a09c3d4f21 CONNECT a b", &cmd));

    /* Token quá dài */
    memset(long_tok, 'a', PROTO_MAX_TOKEN);
    strcpy(long_tok + PROTO_MAX_TOKEN, " INFO");
    ASSERT_EQ_INT(PROTO_BAD_TOKEN, proto_parse_cmd(long_tok, &cmd));

    /* Dòng quá dài hoặc quá nhiều trường */
    memset(long_line, 'x', PROTO_MAX_LINE + 5);
    long_line[PROTO_MAX_LINE + 5] = '\0';
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd(long_line, &cmd));
    ASSERT_EQ_INT(PROTO_BAD_ARG, proto_parse_cmd("tok SET a b c d e f g h i j", &cmd));
}

static void test_check_device_support(void) {
    proto_cmd_t cmd;

    ASSERT_EQ_INT(PROTO_UNSUPPORTED, proto_check_device(NULL, DEV_SENSOR));
    proto_parse_cmd("tok INFO", &cmd);
    ASSERT_EQ_INT(PROTO_UNSUPPORTED, proto_check_device(&cmd, DEV_NONE));
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_SENSOR));
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_PUMP));
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_FERT));
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_LIGHT));

    proto_parse_cmd("tok DATA", &cmd);
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_SENSOR));
    ASSERT_EQ_INT(PROTO_UNSUPPORTED, proto_check_device(&cmd, DEV_PUMP));

    proto_parse_cmd("tok PUMP 2", &cmd);
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_PUMP));
    ASSERT_EQ_INT(PROTO_UNSUPPORTED, proto_check_device(&cmd, DEV_FERT));

    proto_parse_cmd("tok FERTILIZE", &cmd);
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_FERT));
    ASSERT_EQ_INT(PROTO_UNSUPPORTED, proto_check_device(&cmd, DEV_LIGHT));

    proto_parse_cmd("tok LIGHT ON", &cmd);
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_LIGHT));
    ASSERT_EQ_INT(PROTO_UNSUPPORTED, proto_check_device(&cmd, DEV_SENSOR));

    proto_parse_cmd("tok SET INTERVAL 5", &cmd);
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_SENSOR));
    ASSERT_EQ_INT(PROTO_UNSUPPORTED, proto_check_device(&cmd, DEV_PUMP));

    proto_parse_cmd("tok SET HMIN 30", &cmd);
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_PUMP));
    ASSERT_EQ_INT(PROTO_UNSUPPORTED, proto_check_device(&cmd, DEV_FERT));

    proto_parse_cmd("tok SET SCHEDULE 06:00", &cmd);
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_PUMP));
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_LIGHT));
    ASSERT_EQ_INT(PROTO_UNSUPPORTED, proto_check_device(&cmd, DEV_FERT));

    proto_parse_cmd("tok SET VOLUME 2", &cmd);
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_PUMP));
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_FERT));
    ASSERT_EQ_INT(PROTO_UNSUPPORTED, proto_check_device(&cmd, DEV_LIGHT));

    proto_parse_cmd("tok SET NMIN 10", &cmd);
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_FERT));
    ASSERT_EQ_INT(PROTO_UNSUPPORTED, proto_check_device(&cmd, DEV_PUMP));

    proto_parse_cmd("tok SET WATT 100", &cmd);
    ASSERT_EQ_INT(PROTO_OK, proto_check_device(&cmd, DEV_LIGHT));
    ASSERT_EQ_INT(PROTO_UNSUPPORTED, proto_check_device(&cmd, DEV_SENSOR));
}

static void test_format_and_parse_reply(void) {
    char buf[256];
    proto_reply_t rep;

    ASSERT_TRUE(proto_format_reply(buf, sizeof buf, PROTO_OK, NULL) > 0);
    ASSERT_STR_EQ("100 OK\n", buf);
    ASSERT_EQ_INT(0, proto_parse_reply(buf, &rep));
    ASSERT_EQ_INT(PROTO_OK, rep.code);
    ASSERT_EQ_INT(0, rep.is_multi);
    ASSERT_STR_EQ("", rep.detail);

    ASSERT_TRUE(proto_format_replyf(buf, sizeof buf, PROTO_OK, "watered=%.1f", 2.5) > 0);
    ASSERT_STR_EQ("100 OK watered=2.5\n", buf);
    ASSERT_EQ_INT(0, proto_parse_reply(buf, &rep));
    ASSERT_EQ_INT(PROTO_OK, rep.code);
    ASSERT_STR_EQ("watered=2.5", rep.detail);

    ASSERT_TRUE(proto_format_replyf(buf, sizeof buf, PROTO_BAD_PASSWORD, NULL) > 0);
    ASSERT_STR_EQ("200 BAD PASSWORD\n", buf);
    ASSERT_EQ_INT(0, proto_parse_reply(buf, &rep));
    ASSERT_EQ_INT(PROTO_BAD_PASSWORD, rep.code);
    ASSERT_STR_EQ("", rep.detail);

    ASSERT_TRUE(proto_format_multi_header(buf, sizeof buf, "LOG", 2) > 0);
    ASSERT_STR_EQ("150 LOG 2\n", buf);
    ASSERT_EQ_INT(0, proto_parse_reply(buf, &rep));
    ASSERT_EQ_INT(PROTO_MULTI, rep.code);
    ASSERT_EQ_INT(1, rep.is_multi);
    ASSERT_STR_EQ("LOG", rep.multi_tag);
    ASSERT_EQ_INT(2, rep.multi_count);

    ASSERT_TRUE(proto_format_multi_header(buf, sizeof buf, "HISTORY", 0) > 0);
    ASSERT_EQ_INT(0, proto_parse_reply(buf, &rep));
    ASSERT_EQ_INT(0, rep.multi_count);

    /* Kiểm tra lỗi định dạng / bộ đệm nhỏ */
    ASSERT_EQ_INT(-1, proto_format_reply(NULL, 10, PROTO_OK, NULL));
    ASSERT_EQ_INT(-1, proto_format_reply(buf, 4, PROTO_OK, NULL));
    ASSERT_EQ_INT(-1, proto_format_multi_header(buf, sizeof buf, "", 2));
    ASSERT_EQ_INT(-1, proto_format_multi_header(buf, sizeof buf, "LOG", -1));
    ASSERT_EQ_INT(-1, proto_parse_reply(NULL, &rep));
    ASSERT_EQ_INT(-1, proto_parse_reply("100 OK", NULL));
    ASSERT_EQ_INT(-1, proto_parse_reply("", &rep));
    ASSERT_EQ_INT(-1, proto_parse_reply("abc OK", &rep));
    ASSERT_EQ_INT(-1, proto_parse_reply("99 OK", &rep));
    ASSERT_EQ_INT(-1, proto_parse_reply("100   ", &rep));
    ASSERT_EQ_INT(-1, proto_parse_reply("150 LOG", &rep));
    ASSERT_EQ_INT(-1, proto_parse_reply("150 LOG bad", &rep));
}

static void test_scan_and_device_messages(void) {
    char buf[128];
    char id[32];
    dev_type_t dt;
    int port = 0;

    ASSERT_TRUE(proto_format_scan(buf, sizeof buf, "9f3a1c0b7d2e4a58") > 0);
    ASSERT_STR_EQ("SCAN 9f3a1c0b7d2e4a58\n", buf);
    ASSERT_EQ_INT(0, proto_parse_scan(buf, id, sizeof id));
    ASSERT_STR_EQ("9f3a1c0b7d2e4a58", id);

    ASSERT_EQ_INT(-1, proto_format_scan(buf, sizeof buf, ""));
    ASSERT_EQ_INT(-1, proto_parse_scan("SCAN", id, sizeof id));
    ASSERT_EQ_INT(-1, proto_parse_scan("SCAN a b", id, sizeof id));
    ASSERT_EQ_INT(-1, proto_parse_scan("HELLO a", id, sizeof id));
    ASSERT_EQ_INT(-1, proto_parse_scan("SCAN 12345678", id, 4));

    ASSERT_TRUE(proto_format_device(buf, sizeof buf, "P01", DEV_PUMP, 7002) > 0);
    ASSERT_STR_EQ("DEVICE P01 PUMP 7002\n", buf);
    ASSERT_EQ_INT(0, proto_parse_device(buf, id, sizeof id, &dt, &port));
    ASSERT_STR_EQ("P01", id);
    ASSERT_EQ_INT(DEV_PUMP, dt);
    ASSERT_EQ_INT(7002, port);

    ASSERT_EQ_INT(-1, proto_format_device(buf, sizeof buf, "P01", DEV_NONE, 7002));
    ASSERT_EQ_INT(-1, proto_format_device(buf, sizeof buf, "P01", DEV_PUMP, 70000));
    ASSERT_EQ_INT(-1, proto_parse_device("DEVICE P01 UNKNOWN 7002", id, sizeof id, &dt, &port));
    ASSERT_EQ_INT(-1, proto_parse_device("DEVICE P01 PUMP 0", id, sizeof id, &dt, &port));
    ASSERT_EQ_INT(-1, proto_parse_device("DEVICE P01 PUMP", id, sizeof id, &dt, &port));
}

int main(void) {
    printf("test_proto\n");
    RUN_TEST(test_dev_type_conversions);
    RUN_TEST(test_code_tables_and_cmd_names);
    RUN_TEST(test_parse_connect);
    RUN_TEST(test_parse_common_commands);
    RUN_TEST(test_parse_device_specific_commands);
    RUN_TEST(test_parse_set_commands);
    RUN_TEST(test_parse_errors_and_edge_cases);
    RUN_TEST(test_check_device_support);
    RUN_TEST(test_format_and_parse_reply);
    RUN_TEST(test_scan_and_device_messages);
    return MT_SUMMARY();
}
