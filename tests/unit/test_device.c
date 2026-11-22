/* Unit test cho device: khởi tạo, xác thực, lệnh chung. Không dùng mạng. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "device.h"
#include "minitest.h"
#include "store.h"

#define T0 1700000000000LL /* mốc thời gian thật dùng cho mọi test (ms) */

static char g_root[128];
static char g_out[DEVICE_REPLY_MAX];

static void new_root(void) {
    snprintf(g_root, sizeof g_root, "/tmp/device_test_XXXXXX");
    if (mkdtemp(g_root) == NULL) {
        g_root[0] = '\0';
    }
}

static void drop_root(void) {
    char cmd[200];
    if (g_root[0]) {
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", g_root);
        if (system(cmd) != 0) {
            /* chỉ là dọn dẹp */
        }
    }
}

static device_config_t make_cfg(dev_type_t type, const char *id) {
    device_config_t c;
    memset(&c, 0, sizeof c);
    c.type = type;
    snprintf(c.id, sizeof c.id, "%s", id);
    snprintf(c.password, sizeof c.password, "1234");
    snprintf(c.state_dir, sizeof c.state_dir, "%s/state", g_root);
    snprintf(c.log_dir, sizeof c.log_dir, "%s/log", g_root);
    c.timescale = 1.0;
    return c;
}

static int init_dev(device_t *d, dev_type_t type, const char *id) {
    device_config_t c = make_cfg(type, id);
    return device_init(d, &c, T0, NULL, 0);
}

/* Gửi một dòng lệnh tại thời điểm thật real_ms, kết quả vào g_out. */
static int send_at(device_t *d, const char *line, long long real_ms) {
    return device_handle_line(d, line, real_ms, g_out, sizeof g_out);
}

static int send_cmd(device_t *d, const char *line) {
    return send_at(d, line, T0);
}

/* Ghép "<token> <lệnh>" rồi gửi. */
static int send_tok(device_t *d, const char *tok, const char *cmd) {
    char line[256];
    snprintf(line, sizeof line, "%s %s", tok, cmd);
    return send_cmd(d, line);
}

/* CONNECT và lấy token. Trả về mã phản hồi. */
static int connect_app(device_t *d, const char *app, const char *pw, char *token) {
    char line[256];
    int code;
    snprintf(line, sizeof line, "CONNECT %s %s", app, pw);
    send_cmd(d, line);
    code = atoi(g_out);
    if (token) {
        token[0] = '\0';
        if (code == 100 && strlen(g_out) >= 7 + 16) {
            memcpy(token, g_out + 7, 16); /* "100 OK " + 16 ký tự token */
            token[16] = '\0';
        }
    }
    return code;
}

static int starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static int count_lines(const char *s) {
    int n = 0;
    for (; *s; s++) {
        n += (*s == '\n');
    }
    return n;
}

static void test_init_ok_creates_state_and_log(void) {
    device_t d; store_t st; char lines[4][LOG_MAX_LINE];
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d, DEV_PUMP, "P01"));
    ASSERT_EQ_INT(1, d.power);
    ASSERT_STR_EQ("P01", d.id);
    store_init(&st);
    ASSERT_EQ_INT(STORE_OK, store_load(&st, d.state_dir, "P01"));
    ASSERT_STR_EQ("1234", st.password);
    ASSERT_EQ_INT(1, log_tail(&d.log, lines, 4));
    ASSERT_TRUE(strstr(lines[0], "SYS started type=PUMP id=P01") != NULL);
    drop_root();
}

static void test_init_rejects_bad_input(void) {
    device_t d; device_config_t c; char err[200] = "";
    new_root();
    c = make_cfg(DEV_PUMP, "P01");
    ASSERT_EQ_INT(-1, device_init(NULL, &c, T0, NULL, 0));
    ASSERT_EQ_INT(-1, device_init(&d, NULL, T0, NULL, 0));
    c = make_cfg(DEV_NONE, "P01");
    ASSERT_EQ_INT(-1, device_init(&d, &c, T0, err, sizeof err));
    ASSERT_TRUE(err[0] != '\0');
    c = make_cfg(DEV_PUMP, "id sai");
    ASSERT_EQ_INT(-1, device_init(&d, &c, T0, NULL, 0));
    c = make_cfg(DEV_PUMP, "P01");
    c.timescale = 0;
    ASSERT_EQ_INT(-1, device_init(&d, &c, T0, NULL, 0));
    c = make_cfg(DEV_PUMP, "P01");
    c.log_dir[0] = '\0';
    ASSERT_EQ_INT(-1, device_init(&d, &c, T0, NULL, 0));
    c = make_cfg(DEV_PUMP, "P02");
    c.password[0] = '\0'; /* chưa có state và không có mật khẩu */
    ASSERT_EQ_INT(-2, device_init(&d, &c, T0, err, sizeof err));
    ASSERT_TRUE(strstr(err, "--password") != NULL);
    snprintf(c.password, sizeof c.password, "abc"); /* quá ngắn */
    ASSERT_EQ_INT(-2, device_init(&d, &c, T0, NULL, 0));
    drop_root();
}

static void test_init_without_state_dir(void) {
    device_t d; device_config_t c;
    new_root();
    c = make_cfg(DEV_LIGHT, "L01");
    c.state_dir[0] = '\0'; /* không lưu trạng thái */
    ASSERT_EQ_INT(0, device_init(&d, &c, T0, NULL, 0));
    ASSERT_EQ_INT(100, connect_app(&d, "aaaa", "1234", NULL));
    drop_root();
}

static void test_init_state_and_log_errors(void) {
    device_t d; device_config_t c; char err[200] = ""; FILE *f; char path[600];
    new_root();
    /* file trạng thái hỏng */
    c = make_cfg(DEV_PUMP, "P01");
    ASSERT_EQ_INT(0, mkdir(c.state_dir, 0700));
    snprintf(path, sizeof path, "%s/P01.state", c.state_dir);
    f = fopen(path, "w");
    ASSERT_NOT_NULL(f);
    fputs("rac ruoi khong co dau bang\n", f);
    fclose(f);
    ASSERT_EQ_INT(-2, device_init(&d, &c, T0, err, sizeof err));
    ASSERT_TRUE(strstr(err, "hỏng") != NULL);

    /* thư mục log thực ra là một file */
    c = make_cfg(DEV_PUMP, "P02");
    snprintf(path, sizeof path, "%s/la_file", g_root);
    f = fopen(path, "w");
    ASSERT_NOT_NULL(f);
    fclose(f);
    snprintf(c.log_dir, sizeof c.log_dir, "%.250s", path);
    ASSERT_EQ_INT(-2, device_init(&d, &c, T0, err, sizeof err));
    ASSERT_TRUE(strstr(err, "log") != NULL);

    /* thư mục trạng thái nằm dưới một file: không tạo/ghi được */
    c = make_cfg(DEV_PUMP, "P03");
    snprintf(c.state_dir, sizeof c.state_dir, "%.200s/duoi_file", path);
    ASSERT_EQ_INT(-2, device_init(&d, &c, T0, err, sizeof err));
    ASSERT_TRUE(strstr(err, "trạng thái") != NULL);

    /* đường dẫn file log quá dài so với giới hạn của logger */
    c = make_cfg(DEV_PUMP, "P04");
    {
        size_t used = strlen(g_root) + 1;
        size_t n = 250 - used;
        snprintf(c.log_dir, sizeof c.log_dir, "%s/", g_root);
        memset(c.log_dir + used, 'l', n);
        c.log_dir[used + n] = '\0';
    }
    ASSERT_EQ_INT(-2, device_init(&d, &c, T0, err, sizeof err));
    drop_root();
}

static void test_init_uses_saved_state(void) {
    device_t d1, d2; device_config_t c; char tok[17], lines[8][LOG_MAX_LINE];
    int n;
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d1, DEV_PUMP, "P01"));
    ASSERT_EQ_INT(100, connect_app(&d1, "aaaa", "1234", tok));
    ASSERT_TRUE(store_set(&d1.store, "HMIN", "35") == 0);
    send_tok(&d1, tok, "PASSWD 1234 matkhaumoi");
    ASSERT_TRUE(starts_with(g_out, "100 OK"));

    /* khởi động lại với --password cũ: file trạng thái phải thắng */
    c = make_cfg(DEV_PUMP, "P01");
    ASSERT_EQ_INT(0, device_init(&d2, &c, T0, NULL, 0));
    ASSERT_EQ_INT(200, connect_app(&d2, "aaaa", "1234", NULL));
    ASSERT_EQ_INT(100, connect_app(&d2, "aaaa", "matkhaumoi", NULL));
    n = log_tail(&d2.log, lines, 8);
    ASSERT_TRUE(n >= 1);
    {
        int found = 0;
        for (int i = 0; i < n; i++) {
            found |= strstr(lines[i], "(state loaded)") != NULL;
        }
        ASSERT_TRUE(found);
    }
    drop_root();
}

static void test_connect_flow(void) {
    device_t d; char t1[17], t2[17];
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d, DEV_PUMP, "P01"));
    ASSERT_EQ_INT(100, connect_app(&d, "app1", "1234", t1));
    /* "100 OK <token> <id> <type> <version>" */
    ASSERT_TRUE(starts_with(g_out + 7 + 16, " P01 PUMP 1.0\n"));
    ASSERT_EQ_INT(200, connect_app(&d, "app1", "sai", NULL));
    ASSERT_TRUE(starts_with(g_out, "200 BAD PASSWORD"));
    ASSERT_EQ_INT(100, connect_app(&d, "app1", "1234", t2));
    ASSERT_STR_EQ(t1, t2); /* kết nối lại: cấp lại token cũ */
    ASSERT_EQ_INT(202, connect_app(&d, "app_id_qua_dai_17", "1234", NULL));
    send_cmd(&d, "CONNECT onlyone");
    ASSERT_TRUE(starts_with(g_out, "202 BAD ARGUMENT"));
    drop_root();
}

static void test_lockout_and_expiry(void) {
    device_t d; char line[128];
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d, DEV_PUMP, "P01"));
    for (int i = 0; i < SESSION_MAX_FAILS; i++) {
        ASSERT_EQ_INT(200, connect_app(&d, "app1", "sai", NULL));
    }
    ASSERT_EQ_INT(206, connect_app(&d, "app1", "1234", NULL)); /* đúng mật khẩu vẫn bị khóa */
    ASSERT_EQ_INT(100, connect_app(&d, "app2", "1234", NULL)); /* app khác không bị ảnh hưởng */
    /* sau 30 giây (giờ giả lập = giờ thật khi timescale 1) thì mở khóa */
    snprintf(line, sizeof line, "CONNECT app1 1234");
    send_at(&d, line, T0 + (SESSION_LOCK_SEC + 1) * 1000LL);
    ASSERT_EQ_INT(100, atoi(g_out));
    drop_root();
}

static void test_too_many_apps(void) {
    device_t d; char app[16];
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d, DEV_PUMP, "P01"));
    for (int i = 0; i < SESSION_MAX_APPS; i++) {
        snprintf(app, sizeof app, "app%d", i);
        ASSERT_EQ_INT(100, connect_app(&d, app, "1234", NULL));
    }
    ASSERT_EQ_INT(205, connect_app(&d, "appmoi", "1234", NULL));
    drop_root();
}

static void test_token_required_and_syntax_errors(void) {
    device_t d; char tok[17];
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d, DEV_PUMP, "P01"));
    ASSERT_EQ_INT(100, connect_app(&d, "app1", "1234", tok));
    send_cmd(&d, "INFO");
    ASSERT_TRUE(starts_with(g_out, "201 BAD TOKEN"));
    send_cmd(&d, "0000000000000000 INFO"); /* token đúng khuôn dạng nhưng không tồn tại */
    ASSERT_TRUE(starts_with(g_out, "201 BAD TOKEN"));
    send_cmd(&d, "tokensai INFO");
    ASSERT_TRUE(starts_with(g_out, "201 BAD TOKEN"));
    send_tok(&d, tok, "FOO");
    ASSERT_TRUE(starts_with(g_out, "300 UNKNOWN COMMAND"));
    send_cmd(&d, "");
    ASSERT_TRUE(starts_with(g_out, "300 UNKNOWN COMMAND"));
    send_tok(&d, tok, "INFO them");
    ASSERT_TRUE(starts_with(g_out, "202 BAD ARGUMENT"));
    drop_root();
}

static void test_info_status_and_power(void) {
    device_t d; device_config_t c; char tok[17], line[64];
    new_root();
    c = make_cfg(DEV_SENSOR, "S01");
    c.timescale = 60.0;
    ASSERT_EQ_INT(0, device_init(&d, &c, T0, NULL, 0));
    ASSERT_EQ_INT(100, connect_app(&d, "app1", "1234", tok));
    snprintf(line, sizeof line, "%s INFO", tok);
    send_at(&d, line, T0 + 5000); /* 5 giây thật x 60 = 300 giây giả lập */
    ASSERT_STR_EQ("100 OK id=S01 type=SENSOR version=1.0 uptime=300\n", g_out);

    send_tok(&d, tok, "STATUS");
    ASSERT_STR_EQ("100 OK power=ON\n", g_out);
    send_tok(&d, tok, "POWER OFF");
    ASSERT_STR_EQ("100 OK\n", g_out);
    ASSERT_EQ_INT(0, d.power);
    send_tok(&d, tok, "STATUS");
    ASSERT_STR_EQ("100 OK power=OFF\n", g_out);
    send_tok(&d, tok, "POWER ON");
    ASSERT_EQ_INT(1, d.power);
    send_tok(&d, tok, "POWER MAYBE");
    ASSERT_TRUE(starts_with(g_out, "202 BAD ARGUMENT"));
    drop_root();
}

static void test_device_specific_commands(void) {
    device_t d; char tok[17];
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d, DEV_SENSOR, "S01"));
    ASSERT_EQ_INT(100, connect_app(&d, "app1", "1234", tok));
    send_tok(&d, tok, "PUMP 1");           /* không áp dụng cho cảm biến */
    ASSERT_STR_EQ("203 UNSUPPORTED\n", g_out);
    send_tok(&d, tok, "DATA");             /* áp dụng nhưng chưa cài đặt */
    ASSERT_STR_EQ("203 UNSUPPORTED not implemented yet\n", g_out);
    send_tok(&d, tok, "POWER OFF");
    send_tok(&d, tok, "DATA");             /* thiết bị tắt: từ chối lệnh vận hành */
    ASSERT_STR_EQ("204 DEVICE OFF\n", g_out);
    send_tok(&d, tok, "INFO");             /* lệnh chung vẫn dùng được khi tắt */
    ASSERT_TRUE(starts_with(g_out, "100 OK id=S01"));
    send_tok(&d, tok, "PARAMS");
    ASSERT_TRUE(starts_with(g_out, "100 OK"));
    drop_root();
}

static void test_params(void) {
    device_t d; char tok[17], key[16];
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d, DEV_PUMP, "P01"));
    ASSERT_EQ_INT(100, connect_app(&d, "app1", "1234", tok));
    send_tok(&d, tok, "PARAMS");
    ASSERT_STR_EQ("100 OK\n", g_out);
    store_set(&d.store, "HMIN", "30");
    store_set(&d.store, "SCHEDULE", "06:00,17:00");
    send_tok(&d, tok, "PARAMS");
    ASSERT_STR_EQ("100 OK HMIN=30 SCHEDULE=06:00,17:00\n", g_out);

    /* quá nhiều tham số dài: phản hồi vẫn nằm gọn trong một dòng 512 byte */
    for (int i = 0; i < STORE_MAX_KV - 2; i++) {
        char big[STORE_VAL_MAX + 1];
        snprintf(key, sizeof key, "KEY%d", i);
        memset(big, 'v', STORE_VAL_MAX);
        big[STORE_VAL_MAX] = '\0';
        store_set(&d.store, key, big);
    }
    send_tok(&d, tok, "PARAMS");
    ASSERT_TRUE(strlen(g_out) <= PROTO_MAX_LINE + 1);
    ASSERT_EQ_INT(1, count_lines(g_out));
    ASSERT_TRUE(starts_with(g_out, "100 OK HMIN=30 SCHEDULE=06:00,17:00 KEY0="));
    drop_root();
}

static void test_passwd(void) {
    device_t d; char t1[17], t2[17]; store_t st;
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d, DEV_PUMP, "P01"));
    ASSERT_EQ_INT(100, connect_app(&d, "app1", "1234", t1));
    ASSERT_EQ_INT(100, connect_app(&d, "app2", "1234", t2));

    send_tok(&d, t1, "PASSWD sai matkhaumoi");
    ASSERT_TRUE(starts_with(g_out, "200 BAD PASSWORD"));
    send_tok(&d, t1, "PASSWD 1234 abc"); /* mật khẩu mới quá ngắn */
    ASSERT_TRUE(starts_with(g_out, "202 BAD ARGUMENT"));
    ASSERT_TRUE(store_password_matches(&d.store, "1234"));

    send_tok(&d, t1, "PASSWD 1234 matkhaumoi");
    ASSERT_STR_EQ("100 OK\n", g_out);
    send_tok(&d, t1, "INFO"); /* phiên đổi mật khẩu vẫn còn */
    ASSERT_TRUE(starts_with(g_out, "100 OK"));
    send_tok(&d, t2, "INFO"); /* token của ứng dụng khác bị thu hồi */
    ASSERT_TRUE(starts_with(g_out, "201 BAD TOKEN"));
    ASSERT_EQ_INT(200, connect_app(&d, "app2", "1234", NULL));
    ASSERT_EQ_INT(100, connect_app(&d, "app2", "matkhaumoi", NULL));

    store_init(&st); /* đã lưu xuống file */
    ASSERT_EQ_INT(STORE_OK, store_load(&st, d.state_dir, "P01"));
    ASSERT_STR_EQ("matkhaumoi", st.password);
    drop_root();
}

static void test_passwd_save_failure_keeps_old_password(void) {
    device_t d; char tok[17], path[256]; FILE *f;
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d, DEV_PUMP, "P01"));
    ASSERT_EQ_INT(100, connect_app(&d, "app1", "1234", tok));
    snprintf(path, sizeof path, "%s/la_file", g_root);
    f = fopen(path, "w");
    ASSERT_NOT_NULL(f);
    fclose(f);
    snprintf(d.state_dir, sizeof d.state_dir, "%s", path); /* không thể lưu vào đây */
    send_tok(&d, tok, "PASSWD 1234 matkhaumoi");
    ASSERT_STR_EQ("999 UNKNOWN ERROR cannot save state\n", g_out);
    ASSERT_TRUE(store_password_matches(&d.store, "1234"));
    drop_root();
}

static void test_log_command(void) {
    device_t d; char tok[17], line[64];
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d, DEV_PUMP, "P01"));
    ASSERT_EQ_INT(200, connect_app(&d, "app1", "sai", NULL));
    ASSERT_EQ_INT(100, connect_app(&d, "app1", "1234", tok));
    send_tok(&d, tok, "POWER OFF");
    send_tok(&d, tok, "PASSWD 1234 matkhaumoi");

    send_tok(&d, tok, "LOG");
    ASSERT_TRUE(starts_with(g_out, "150 LOG 5\n")); /* SYS + 2 AUTH + CMD + AUTH */
    ASSERT_EQ_INT(6, count_lines(g_out));
    ASSERT_TRUE(strstr(g_out, " SYS started type=PUMP") != NULL);
    ASSERT_TRUE(strstr(g_out, " AUTH app=app1 wrong password") != NULL);
    ASSERT_TRUE(strstr(g_out, " CMD app=app1 POWER OFF") != NULL);
    ASSERT_TRUE(strstr(g_out, "password changed") != NULL);
    /* mật khẩu không bao giờ được ghi vào log */
    ASSERT_TRUE(strstr(g_out, "matkhaumoi") == NULL);
    ASSERT_TRUE(strstr(g_out, "1234") == NULL);

    send_tok(&d, tok, "LOG 2");
    ASSERT_TRUE(starts_with(g_out, "150 LOG 2\n"));
    ASSERT_EQ_INT(3, count_lines(g_out));
    send_tok(&d, tok, "LOG 5000"); /* vượt mức cho phép: bị giới hạn nhưng vẫn trả đủ dòng có */
    ASSERT_TRUE(starts_with(g_out, "150 LOG "));
    snprintf(line, sizeof line, "%s LOG 0", tok);
    send_cmd(&d, line);
    ASSERT_TRUE(starts_with(g_out, "202 BAD ARGUMENT"));
    drop_root();
}

static void test_log_many_lines_and_errors(void) {
    device_t d; char tok[17];
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d, DEV_PUMP, "P01"));
    ASSERT_EQ_INT(100, connect_app(&d, "app1", "1234", tok));
    for (int i = 0; i < DEVICE_LOG_MAX_LINES + 20; i++) {
        device_log(&d, T0, LOG_SRC_AUTO, "dong log thu nghiem");
    }
    send_tok(&d, tok, "LOG 5000");
    ASSERT_TRUE(starts_with(g_out, "150 LOG 100\n"));
    ASSERT_EQ_INT(101, count_lines(g_out));

    d.log.path[0] = '\0'; /* log không đọc được */
    send_tok(&d, tok, "LOG");
    ASSERT_TRUE(starts_with(g_out, "999 UNKNOWN ERROR"));
    device_log(NULL, T0, LOG_SRC_SYS, "bo qua");
    device_log(&d, T0, LOG_SRC_SYS, NULL);
    drop_root();
}

static void test_output_buffer_too_small(void) {
    device_t d; char tok[17], small[8], line[64];
    new_root();
    ASSERT_EQ_INT(0, init_dev(&d, DEV_PUMP, "P01"));
    ASSERT_EQ_INT(100, connect_app(&d, "app1", "1234", tok));
    snprintf(line, sizeof line, "%s INFO", tok);
    ASSERT_EQ_INT(-1, device_handle_line(&d, line, T0, small, sizeof small));
    snprintf(line, sizeof line, "%s LOG", tok);
    ASSERT_EQ_INT(-1, device_handle_line(&d, line, T0, g_out, 20)); /* không đủ chỗ cho dòng log */
    ASSERT_EQ_INT(-1, device_handle_line(NULL, line, T0, g_out, sizeof g_out));
    ASSERT_EQ_INT(-1, device_handle_line(&d, NULL, T0, g_out, sizeof g_out));
    ASSERT_EQ_INT(-1, device_handle_line(&d, line, T0, NULL, 10));
    ASSERT_EQ_INT(-1, device_handle_line(&d, line, T0, g_out, 0));
    drop_root();
}

int main(void) {
    printf("test_device\n");
    RUN_TEST(test_init_ok_creates_state_and_log);
    RUN_TEST(test_init_rejects_bad_input);
    RUN_TEST(test_init_without_state_dir);
    RUN_TEST(test_init_state_and_log_errors);
    RUN_TEST(test_init_uses_saved_state);
    RUN_TEST(test_connect_flow);
    RUN_TEST(test_lockout_and_expiry);
    RUN_TEST(test_too_many_apps);
    RUN_TEST(test_token_required_and_syntax_errors);
    RUN_TEST(test_info_status_and_power);
    RUN_TEST(test_device_specific_commands);
    RUN_TEST(test_params);
    RUN_TEST(test_passwd);
    RUN_TEST(test_passwd_save_failure_keeps_old_password);
    RUN_TEST(test_log_command);
    RUN_TEST(test_log_many_lines_and_errors);
    RUN_TEST(test_output_buffer_too_small);
    return MT_SUMMARY();
}
