/*
 * Integration test: kết hợp các module phía controller (appid, garden, scan, client)
 * với thiết bị thật (device_t + server_t) chạy trên 127.0.0.1.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "appid.h"
#include "client.h"
#include "device.h"
#include "garden.h"
#include "minitest.h"
#include "scan.h"
#include "server.h"

static device_t g_dev;
static server_t g_srv;
static char     g_root[128];
static volatile int g_running = 0;
static pthread_t g_thread;

static void *server_loop(void *arg) {
    (void)arg;
    while (g_running) {
        server_step(&g_srv, 5);
    }
    return NULL;
}

static int start_real_device(dev_type_t type, const char *id, const char *pw) {
    device_config_t cfg;

    snprintf(g_root, sizeof g_root, "/tmp/ctrldev_test_XXXXXX");
    if (mkdtemp(g_root) == NULL) {
        g_root[0] = '\0';
        return -1;
    }
    memset(&cfg, 0, sizeof cfg);
    cfg.type = type;
    snprintf(cfg.id, sizeof cfg.id, "%s", id);
    snprintf(cfg.password, sizeof cfg.password, "%s", pw);
    snprintf(cfg.state_dir, sizeof cfg.state_dir, "%s/state", g_root);
    snprintf(cfg.log_dir, sizeof cfg.log_dir, "%s/log", g_root);
    cfg.timescale = 1.0;

    if (device_init(&g_dev, &cfg, 1700000000000LL, NULL, 0) != 0) {
        return -1;
    }
    if (server_open(&g_srv, &g_dev, "127.0.0.1", 0, 0) != 0) {
        return -1;
    }
    g_running = 1;
    if (pthread_create(&g_thread, NULL, server_loop, NULL) != 0) {
        g_running = 0;
        server_close(&g_srv);
        return -1;
    }
    return 0;
}

static void stop_real_device(void) {
    char cmd[200];

    if (g_running) {
        g_running = 0;
        pthread_join(g_thread, NULL);
    }
    server_close(&g_srv);
    if (g_root[0]) {
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", g_root);
        if (system(cmd) != 0) {
            /* chỉ dọn thư mục tạm */
        }
        g_root[0] = '\0';
    }
}

static void test_scan_connect_commands_and_garden_persist(void) {
    char ctrl_dir[160];
    char app_id[APPID_BUF_SIZE];
    scan_device_t found[8];
    garden_list_t gardens, loaded;
    garden_t *g;
    int gid = 0;
    dev_conn_t conn;
    dev_reply_t rep;
    int n;

    ASSERT_EQ_INT(0, start_real_device(DEV_PUMP, "P01", "1234"));
    snprintf(ctrl_dir, sizeof ctrl_dir, "%s/ctrl", g_root);

    /* 1. Sinh app_id cho controller */
    ASSERT_EQ_INT(0, appid_get(ctrl_dir, app_id, sizeof app_id));

    /* 2. Quét thiết bị qua UDP trên cổng scan của server */
    n = scan_devices("127.0.0.1", g_srv.scan_port, app_id, 150, found, 8);
    ASSERT_EQ_INT(1, n);
    ASSERT_STR_EQ("P01", found[0].device_id);
    ASSERT_EQ_INT(DEV_PUMP, found[0].type);
    ASSERT_EQ_INT(g_srv.tcp_port, found[0].tcp_port);

    /* 3. Tạo vườn và thêm thiết bị vừa quét được vào vườn */
    garden_list_init(&gardens);
    ASSERT_EQ_INT(0, garden_add(&gardens, "Vườn Thử Nghiệm", "Cải xanh", 20.0, "", &gid));
    g = garden_find(&gardens, gid);
    ASSERT_NOT_NULL(g);
    ASSERT_EQ_INT(0, garden_add_device(g, found[0].device_id, found[0].type,
                                       found[0].host, found[0].tcp_port, NULL));

    /* 4. Kết nối sai mật khẩu -> 200 BAD PASSWORD */
    ASSERT_EQ_INT(PROTO_BAD_PASSWORD,
                  dev_connect(&conn, found[0].host, found[0].tcp_port,
                              app_id, "sai_mk", 1000, &rep));

    /* 5. Kết nối đúng mật khẩu -> 100 OK, lưu lại token vào vườn */
    ASSERT_EQ_INT(PROTO_OK,
                  dev_connect(&conn, found[0].host, found[0].tcp_port,
                              app_id, "1234", 1000, &rep));
    ASSERT_STR_EQ("P01", conn.device_id);
    ASSERT_EQ_INT(DEV_PUMP, conn.type);
    ASSERT_EQ_INT(0, garden_set_device_token(g, "P01", conn.token));
    ASSERT_EQ_INT(0, garden_save(&gardens, ctrl_dir));
    ASSERT_EQ_INT(GARDEN_OK, garden_load(&loaded, ctrl_dir));
    ASSERT_STR_EQ(conn.token, garden_find_device(garden_find(&loaded, gid), "P01")->token);

    /* 6. Gửi các lệnh chung: INFO, STATUS, POWER OFF, POWER ON, LOG */
    ASSERT_EQ_INT(PROTO_OK, dev_send_cmd(&conn, "INFO", &rep));
    ASSERT_TRUE(strstr(rep.detail, "id=P01") != NULL);

    ASSERT_EQ_INT(PROTO_OK, dev_send_cmd(&conn, "POWER OFF", &rep));
    ASSERT_EQ_INT(PROTO_OK, dev_send_cmd(&conn, "STATUS", &rep));
    ASSERT_STR_EQ("power=OFF", rep.detail);

    ASSERT_EQ_INT(PROTO_OK, dev_send_cmd(&conn, "POWER ON", &rep));
    ASSERT_EQ_INT(PROTO_MULTI, dev_send_cmd(&conn, "LOG 5", &rep));
    ASSERT_EQ_INT(1, rep.is_multi);
    ASSERT_STR_EQ("LOG", rep.multi_tag);
    ASSERT_TRUE(rep.line_count >= 3);

    dev_close(&conn);
    stop_real_device();
}

static void test_passwd_change_revokes_other_app_token(void) {
    dev_conn_t c1, c2;
    dev_reply_t rep;
    char saved_tok2[PROTO_MAX_TOKEN];

    ASSERT_EQ_INT(0, start_real_device(DEV_SENSOR, "S01", "1234"));

    /* Hai ứng dụng cùng xác thực vào thiết bị S01 */
    ASSERT_EQ_INT(PROTO_OK,
                  dev_connect(&c1, "127.0.0.1", g_srv.tcp_port,
                              "1111111111111111", "1234", 1000, &rep));
    ASSERT_EQ_INT(PROTO_OK,
                  dev_connect(&c2, "127.0.0.1", g_srv.tcp_port,
                              "2222222222222222", "1234", 1000, &rep));
    strcpy(saved_tok2, c2.token);
    dev_close(&c2);

    /* Ứng dụng 1 đổi mật khẩu -> thu hồi token của ứng dụng 2 */
    ASSERT_EQ_INT(PROTO_OK, dev_send_cmd(&c1, "PASSWD 1234 newpass99", &rep));

    /* Ứng dụng 2 mở lại kết nối bằng token cũ đã lưu -> bị từ chối 201 BAD TOKEN */
    ASSERT_EQ_INT(0, dev_open_with_token(&c2, "127.0.0.1", g_srv.tcp_port, saved_tok2, 1000));
    ASSERT_EQ_INT(PROTO_BAD_TOKEN, dev_send_cmd(&c2, "INFO", &rep));
    dev_close(&c2);

    /* Ứng dụng 2 kết nối lại bằng mật khẩu mới -> thành công và dùng được bình thường */
    ASSERT_EQ_INT(PROTO_OK,
                  dev_connect(&c2, "127.0.0.1", g_srv.tcp_port,
                              "2222222222222222", "newpass99", 1000, &rep));
    ASSERT_EQ_INT(PROTO_OK, dev_send_cmd(&c2, "INFO", &rep));

    dev_close(&c1);
    dev_close(&c2);
    stop_real_device();
}

int main(void) {
    printf("test_controller_device\n");
    RUN_TEST(test_scan_connect_commands_and_garden_persist);
    RUN_TEST(test_passwd_change_revokes_other_app_token);
    return MT_SUMMARY();
}
