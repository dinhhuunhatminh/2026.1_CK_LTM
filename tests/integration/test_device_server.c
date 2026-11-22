/*
 * Integration test: device_sim thật trên 127.0.0.1 (cổng do hệ điều hành cấp).
 * Chạy trong một tiến trình: test gọi server_step() từng bước xen kẽ với thao tác của
 * client, nên kết quả xác định và không cần thread hay sleep dài.
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "device.h"
#include "minitest.h"
#include "netutil.h"
#include "server.h"

static device_t g_dev;
static server_t g_srv;
static char g_root[128];

static void drop_root(void) {
    char cmd[200];
    if (g_root[0]) {
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", g_root);
        if (system(cmd) != 0) {
            /* chỉ là dọn dẹp */
        }
    }
}

/* Khởi tạo thiết bị bơm P01 và server trên cổng ngẫu nhiên. Trả về 0 nếu thành công. */
static int setup(int scan_port) {
    device_config_t c;
    snprintf(g_root, sizeof g_root, "/tmp/devsrv_test_XXXXXX");
    if (mkdtemp(g_root) == NULL) {
        g_root[0] = '\0';
        return -1;
    }
    memset(&c, 0, sizeof c);
    c.type = DEV_PUMP;
    snprintf(c.id, sizeof c.id, "P01");
    snprintf(c.password, sizeof c.password, "1234");
    snprintf(c.state_dir, sizeof c.state_dir, "%s/state", g_root);
    snprintf(c.log_dir, sizeof c.log_dir, "%s/log", g_root);
    c.timescale = 1.0;
    if (device_init(&g_dev, &c, 1700000000000LL, NULL, 0) != 0) {
        return -1;
    }
    return server_open(&g_srv, &g_dev, "127.0.0.1", 0, scan_port);
}

static void teardown(void) {
    server_close(&g_srv);
    drop_root();
}

static int tcp_client(void) {
    return net_tcp_connect("127.0.0.1", g_srv.tcp_port, 1000);
}

static void pump(int rounds) {
    for (int i = 0; i < rounds; i++) {
        server_step(&g_srv, 10);
    }
}

/*
 * Đọc từ fd cho tới khi đủ want dòng, xen kẽ chạy server. Trả về số byte đọc được,
 * -1 nếu bên kia đóng kết nối, -2 nếu hết thời gian chờ.
 */
static int read_lines(int fd, char *buf, size_t cap, int want) {
    size_t len = 0;
    buf[0] = '\0';
    for (int tries = 0; tries < 60; tries++) {
        int n;
        server_step(&g_srv, 10);
        n = net_recv_timeout(fd, buf + len, cap - 1 - len, 0);
        if (n == 0) {
            return -1;
        }
        if (n > 0) {
            int lines = 0;
            len += (size_t)n;
            buf[len] = '\0';
            for (size_t i = 0; i < len; i++) {
                lines += (buf[i] == '\n');
            }
            if (lines >= want) {
                return (int)len;
            }
        }
    }
    return -2;
}

static int ask(int fd, const char *line, char *buf, size_t cap, int want) {
    if (net_send_str(fd, line) != 0) {
        return -3;
    }
    return read_lines(fd, buf, cap, want);
}

/* CONNECT bằng app_id và mật khẩu cho trước; trả về mã phản hồi và token (nếu thành công). */
static int auth(int fd, const char *app, const char *pw, char *token) {
    char line[128], buf[512];
    int n;
    snprintf(line, sizeof line, "CONNECT %s %s\n", app, pw);
    n = ask(fd, line, buf, sizeof buf, 1);
    if (n <= 0) {
        return -1;
    }
    if (token) {
        token[0] = '\0';
        if (atoi(buf) == 100) {
            memcpy(token, buf + 7, 16);
            token[16] = '\0';
        }
    }
    return atoi(buf);
}

static int starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static int udp_socket(void) {
    return socket(AF_INET, SOCK_DGRAM, 0);
}

static void udp_send(int fd, int port, const char *msg) {
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    (void)sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&a, sizeof a);
}

/* Chạy server và chờ một gói UDP trả lời; trả về số byte, hoặc NET_ERR_TIMEOUT. */
static int udp_expect(int fd, char *buf, size_t cap) {
    for (int tries = 0; tries < 20; tries++) {
        int n;
        server_step(&g_srv, 10);
        n = net_recv_timeout(fd, buf, cap - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            return n;
        }
    }
    return NET_ERR_TIMEOUT;
}

static void test_scan_replies_with_device_info(void) {
    int u; char buf[128], expect[64];
    ASSERT_EQ_INT(0, setup(0));
    ASSERT_TRUE(g_srv.tcp_port > 0);
    ASSERT_TRUE(g_srv.scan_port > 0);
    u = udp_socket();
    ASSERT_TRUE(u >= 0);
    udp_send(u, g_srv.scan_port, "SCAN 9f3a1c0b7d2e4a58\n");
    ASSERT_TRUE(udp_expect(u, buf, sizeof buf) > 0);
    snprintf(expect, sizeof expect, "DEVICE P01 PUMP %d\n", g_srv.tcp_port);
    ASSERT_STR_EQ(expect, buf);
    net_close(u);
    teardown();
}

static void test_scan_ignores_malformed_packets(void) {
    int u; char buf[128];
    const char *bad[] = {"SCAN\n", "HELLO abc\n", "SCAN co khoang\n", "SCAN idquadai_17_kitu_\n",
                         "scan abc\n", "\n", "SCAN abc xyz\n"};
    ASSERT_EQ_INT(0, setup(0));
    u = udp_socket();
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        udp_send(u, g_srv.scan_port, bad[i]);
    }
    pump(3);
    ASSERT_EQ_INT(NET_ERR_TIMEOUT, net_recv_timeout(u, buf, sizeof buf, 30)); /* không trả lời gói rác */
    udp_send(u, g_srv.scan_port, "SCAN abc\n"); /* gói đúng sau đó vẫn được trả lời */
    ASSERT_TRUE(udp_expect(u, buf, sizeof buf) > 0);
    ASSERT_TRUE(starts_with(buf, "DEVICE P01 PUMP "));
    net_close(u);
    teardown();
}

static void test_full_session_over_tcp(void) {
    int fd; char tok[17], line[128], buf[8192];
    ASSERT_EQ_INT(0, setup(-1));
    fd = tcp_client();
    ASSERT_TRUE(fd >= 0);
    ASSERT_EQ_INT(200, auth(fd, "app1", "sai", NULL));
    ASSERT_EQ_INT(100, auth(fd, "app1", "1234", tok));

    snprintf(line, sizeof line, "%s INFO\n", tok);
    ASSERT_TRUE(ask(fd, line, buf, sizeof buf, 1) > 0);
    ASSERT_TRUE(starts_with(buf, "100 OK id=P01 type=PUMP version=1.0 uptime="));
    snprintf(line, sizeof line, "%s POWER OFF\n", tok);
    ASSERT_TRUE(ask(fd, line, buf, sizeof buf, 1) > 0);
    ASSERT_STR_EQ("100 OK\n", buf);
    snprintf(line, sizeof line, "%s STATUS\n", tok);
    ASSERT_TRUE(ask(fd, line, buf, sizeof buf, 1) > 0);
    ASSERT_STR_EQ("100 OK power=OFF\n", buf);
    snprintf(line, sizeof line, "%s PUMP 2\n", tok);
    ASSERT_TRUE(ask(fd, line, buf, sizeof buf, 1) > 0);
    ASSERT_STR_EQ("204 DEVICE OFF\n", buf);

    snprintf(line, sizeof line, "%s LOG 3\n", tok); /* phản hồi nhiều dòng: header + 3 dòng */
    ASSERT_TRUE(ask(fd, line, buf, sizeof buf, 4) > 0);
    ASSERT_TRUE(starts_with(buf, "150 LOG 3\n"));
    net_close(fd);
    teardown();
}

static void test_pipelined_and_split_commands(void) {
    int fd; char tok[17], line[256], buf[1024];
    ASSERT_EQ_INT(0, setup(-1));
    fd = tcp_client();
    ASSERT_EQ_INT(100, auth(fd, "app1", "1234", tok));

    /* hai lệnh dính nhau trong một lần gửi -> hai phản hồi theo đúng thứ tự */
    snprintf(line, sizeof line, "%s STATUS\n%s INFO\n", tok, tok);
    ASSERT_TRUE(ask(fd, line, buf, sizeof buf, 2) > 0);
    ASSERT_TRUE(starts_with(buf, "100 OK power=ON\n100 OK id=P01"));

    /* một lệnh gửi từng byte một -> vẫn chỉ một phản hồi */
    snprintf(line, sizeof line, "%s STATUS\r\n", tok); /* kết thúc bằng \r\n cũng được chấp nhận */
    for (size_t i = 0; i < strlen(line); i++) {
        ASSERT_EQ_INT(0, net_send_all(fd, line + i, 1));
        pump(1);
    }
    ASSERT_TRUE(read_lines(fd, buf, sizeof buf, 1) > 0);
    ASSERT_STR_EQ("100 OK power=ON\n", buf);
    net_close(fd);
    teardown();
}

static void test_bad_lines_do_not_break_connection(void) {
    int fd; char big[700], buf[256];
    ASSERT_EQ_INT(0, setup(-1));
    fd = tcp_client();
    memset(big, 'a', 600);
    memcpy(big + 600, "\n", 2);
    ASSERT_TRUE(ask(fd, big, buf, sizeof buf, 1) > 0);
    ASSERT_STR_EQ("202 BAD ARGUMENT line too long\n", buf);
    ASSERT_EQ_INT(0, net_send_all(fd, "ab\0cd\n", 6)); /* dòng chứa byte '\0' */
    ASSERT_TRUE(read_lines(fd, buf, sizeof buf, 1) > 0);
    ASSERT_STR_EQ("202 BAD ARGUMENT invalid byte\n", buf);
    ASSERT_TRUE(ask(fd, "xyz\n", buf, sizeof buf, 1) > 0); /* lệnh lạ */
    ASSERT_TRUE(starts_with(buf, "300 UNKNOWN COMMAND"));
    ASSERT_EQ_INT(100, auth(fd, "app1", "1234", NULL)); /* kết nối vẫn dùng bình thường */
    net_close(fd);
    teardown();
}

static void test_passwd_revokes_other_clients_over_tcp(void) {
    int f1, f2; char t1[17], t2[17], line[128], buf[256];
    ASSERT_EQ_INT(0, setup(-1));
    f1 = tcp_client();
    f2 = tcp_client();
    ASSERT_EQ_INT(100, auth(f1, "app1", "1234", t1));
    ASSERT_EQ_INT(100, auth(f2, "app2", "1234", t2));
    ASSERT_TRUE(strcmp(t1, t2) != 0);
    snprintf(line, sizeof line, "%s PASSWD 1234 matkhaumoi\n", t1);
    ASSERT_TRUE(ask(f1, line, buf, sizeof buf, 1) > 0);
    ASSERT_STR_EQ("100 OK\n", buf);
    snprintf(line, sizeof line, "%s INFO\n", t2);
    ASSERT_TRUE(ask(f2, line, buf, sizeof buf, 1) > 0);
    ASSERT_TRUE(starts_with(buf, "201 BAD TOKEN"));
    ASSERT_EQ_INT(100, auth(f2, "app2", "matkhaumoi", NULL));
    net_close(f1);
    net_close(f2);
    teardown();
}

static void test_client_disconnect_is_detected(void) {
    int fd[3]; int tries;
    ASSERT_EQ_INT(0, setup(-1));
    for (int i = 0; i < 3; i++) {
        fd[i] = tcp_client();
    }
    for (tries = 0; tries < 20 && g_srv.nclients < 3; tries++) {
        pump(1);
    }
    ASSERT_EQ_INT(3, g_srv.nclients);
    net_close(fd[1]);
    for (tries = 0; tries < 20 && g_srv.nclients > 2; tries++) {
        pump(1);
    }
    ASSERT_EQ_INT(2, g_srv.nclients);
    net_close(fd[0]);
    net_close(fd[2]);
    for (tries = 0; tries < 20 && g_srv.nclients > 0; tries++) {
        pump(1);
    }
    ASSERT_EQ_INT(0, g_srv.nclients);
    teardown();
}

static void test_client_limit(void) {
    int fd[SERVER_MAX_CLIENTS], extra; char buf[64], lines[8][LOG_MAX_LINE]; int n, found = 0;
    ASSERT_EQ_INT(0, setup(-1));
    for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
        fd[i] = tcp_client();
        ASSERT_TRUE(fd[i] >= 0);
    }
    for (int tries = 0; tries < 30 && g_srv.nclients < SERVER_MAX_CLIENTS; tries++) {
        pump(1);
    }
    ASSERT_EQ_INT(SERVER_MAX_CLIENTS, g_srv.nclients);
    extra = tcp_client(); /* kết nối thứ 65 bị server đóng ngay */
    ASSERT_TRUE(extra >= 0);
    ASSERT_EQ_INT(-1, read_lines(extra, buf, sizeof buf, 1));
    ASSERT_EQ_INT(SERVER_MAX_CLIENTS, g_srv.nclients);
    n = log_tail(&g_dev.log, lines, 8);
    for (int i = 0; i < n; i++) {
        found |= strstr(lines[i], "client limit reached") != NULL;
    }
    ASSERT_TRUE(found);
    net_close(extra);
    for (int i = 0; i < SERVER_MAX_CLIENTS; i++) {
        net_close(fd[i]);
    }
    teardown();
}

static void test_idle_client_is_closed(void) {
    int fd; char buf[64];
    ASSERT_EQ_INT(0, setup(-1));
    g_srv.idle_ms = 1;
    fd = tcp_client();
    for (int tries = 0; tries < 20 && g_srv.nclients < 1; tries++) {
        pump(1);
    }
    ASSERT_EQ_INT(1, g_srv.nclients);
    usleep(20 * 1000);
    pump(2);
    ASSERT_EQ_INT(0, g_srv.nclients);
    ASSERT_EQ_INT(-1, read_lines(fd, buf, sizeof buf, 1)); /* phía client thấy bị đóng */
    net_close(fd);
    teardown();
}

static void test_open_errors(void) {
    server_t s2; device_t dummy; int blocker, scan_taken;
    ASSERT_EQ_INT(0, setup(-1));
    ASSERT_EQ_INT(-1, server_open(NULL, &g_dev, "127.0.0.1", 0, -1));
    ASSERT_EQ_INT(-1, server_open(&s2, NULL, "127.0.0.1", 0, -1));
    ASSERT_EQ_INT(-1, server_open(&s2, &g_dev, "khong-phai-ip", 0, -1));
    ASSERT_EQ_INT(-1, server_open(&s2, &g_dev, "127.0.0.1", g_srv.tcp_port, -1)); /* cổng TCP đã bị chiếm */

    /* cổng UDP quét bị một socket không cho dùng chung chiếm */
    blocker = net_udp_bind("127.0.0.1", 0, 0);
    ASSERT_TRUE(blocker >= 0);
    scan_taken = net_bound_port(blocker);
    ASSERT_EQ_INT(-1, server_open(&s2, &g_dev, "127.0.0.1", 0, scan_taken));
    net_close(blocker);

    ASSERT_EQ_INT(-1, server_step(NULL, 0));
    memset(&dummy, 0, sizeof dummy);
    server_close(NULL);
    server_close(&g_srv);
    server_close(&g_srv); /* gọi lần hai vẫn an toàn */
    ASSERT_EQ_INT(-1, server_step(&g_srv, 0)); /* server đã đóng */
    drop_root();
}

int main(void) {
    printf("test_device_server\n");
    RUN_TEST(test_scan_replies_with_device_info);
    RUN_TEST(test_scan_ignores_malformed_packets);
    RUN_TEST(test_full_session_over_tcp);
    RUN_TEST(test_pipelined_and_split_commands);
    RUN_TEST(test_bad_lines_do_not_break_connection);
    RUN_TEST(test_passwd_revokes_other_clients_over_tcp);
    RUN_TEST(test_client_disconnect_is_detected);
    RUN_TEST(test_client_limit);
    RUN_TEST(test_idle_client_is_closed);
    RUN_TEST(test_open_errors);
    return MT_SUMMARY();
}
