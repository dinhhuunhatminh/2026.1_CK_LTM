/*
 * Unit test cho scan và client với thiết bị giả trên loopback:
 *   - Gói UDP rác, trùng device_id, hết thời gian chờ.
 *   - Phản hồi TCP bị tách nửa dòng, phản hồi 150 nhiều dòng tách mảnh,
 *     server đóng kết nối giữa chừng, hết thời gian chờ, mã lỗi 200/201/205/206.
 */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "client.h"
#include "minitest.h"
#include "netutil.h"
#include "scan.h"

typedef struct {
    int udp_fd;
} fake_udp_ctx_t;

/* Thiết bị UDP giả: nhận gói SCAN, gửi trả gói rác + gói trùng ID + gói hợp lệ. */
static void *fake_udp_responder(void *arg) {
    fake_udp_ctx_t *ctx = (fake_udp_ctx_t *)arg;
    struct sockaddr_in from;
    socklen_t flen = (socklen_t)sizeof(from);
    char buf[128];
    const char *replies[] = {
        "GARBAGE PACKET\n",
        "DEVICE bad id PUMP 7002\n",
        "DEVICE P01 PUMP 7002\n",
        "DEVICE P01 PUMP 9999\n",   /* trùng device_id P01 -> phải bị bỏ qua */
        "DEVICE S01 SENSOR 7001\n"
    };

    memset(&from, 0, sizeof from);
#ifdef _WIN32
    if (recvfrom((SOCKET)ctx->udp_fd, buf, (int)(sizeof(buf) - 1), 0,
                 (struct sockaddr *)&from, &flen) > 0) {
        for (size_t i = 0; i < sizeof(replies) / sizeof(replies[0]); i++) {
            sendto((SOCKET)ctx->udp_fd, replies[i], (int)strlen(replies[i]), 0,
                   (struct sockaddr *)&from, flen);
        }
    }
#else
    if (recvfrom(ctx->udp_fd, buf, sizeof(buf) - 1, 0,
                 (struct sockaddr *)&from, &flen) > 0) {
        for (size_t i = 0; i < sizeof(replies) / sizeof(replies[0]); i++) {
            (void)sendto(ctx->udp_fd, replies[i], strlen(replies[i]), 0,
                         (struct sockaddr *)&from, flen);
        }
    }
#endif
    return NULL;
}

static void test_scan_dedup_and_garbage_packets(void) {
    fake_udp_ctx_t ctx;
    pthread_t th;
    scan_device_t list[8];
    int port;
    int n;

    ASSERT_EQ_INT(-1, scan_send("bad_ip", 9999, "9f3a1c0b7d2e4a58"));
    ASSERT_EQ_INT(-1, scan_send("127.0.0.1", 70000, "9f3a1c0b7d2e4a58"));
    ASSERT_EQ_INT(-1, scan_send("127.0.0.1", 9999, NULL));
    ASSERT_EQ_INT(-1, scan_send("127.0.0.1", 9999, "bad id space"));
    ASSERT_EQ_INT(-1, scan_collect(-1, 50, list, 8));
    ASSERT_EQ_INT(-1, scan_collect(1, 50, NULL, 8));
    ASSERT_EQ_INT(-1, scan_collect(1, 50, list, 0));
    ASSERT_EQ_INT(-1, scan_devices("127.0.0.1", 9999, "bad id", 50, list, 8));
    ASSERT_EQ_INT(-1, scan_devices("127.0.0.1", 9999, "abc", 50, NULL, 8));

    ctx.udp_fd = net_udp_bind("127.0.0.1", 0, 1);
    ASSERT_TRUE(ctx.udp_fd >= 0);
    port = net_bound_port(ctx.udp_fd);
    ASSERT_TRUE(port > 0);

    ASSERT_EQ_INT(0, pthread_create(&th, NULL, fake_udp_responder, &ctx));
    n = scan_devices("127.0.0.1", port, "9f3a1c0b7d2e4a58", 150, list, 8);
    pthread_join(th, NULL);
    net_close(ctx.udp_fd);

    ASSERT_EQ_INT(2, n);
    ASSERT_STR_EQ("P01", list[0].device_id);
    ASSERT_EQ_INT(DEV_PUMP, list[0].type);
    ASSERT_EQ_INT(7002, list[0].tcp_port);
    ASSERT_STR_EQ("127.0.0.1", list[0].host);

    ASSERT_STR_EQ("S01", list[1].device_id);
    ASSERT_EQ_INT(DEV_SENSOR, list[1].type);
    ASSERT_EQ_INT(7001, list[1].tcp_port);
}

typedef struct {
    int listen_fd;
    int mode; /* 0: tách mảnh + 150 LOG; 1: đóng giữa chừng; 2: trả 200 BAD PASSWORD */
} fake_tcp_ctx_t;

static void *fake_tcp_server(void *arg) {
    fake_tcp_ctx_t *ctx = (fake_tcp_ctx_t *)arg;
    char rbuf[256];
    int cfd = net_tcp_accept(ctx->listen_fd, NULL, 0, NULL);

    if (cfd < 0) {
        return NULL;
    }
    /* Đọc dòng CONNECT từ client */
    (void)net_recv_timeout(cfd, rbuf, sizeof rbuf, 1000);

    if (ctx->mode == 0) {
        /* Gửi phản hồi CONNECT bị tách làm 2 mảnh */
        net_send_str(cfd, "100 OK 5b1e77a09c3d4f21 ");
        net_send_str(cfd, "P01 PUMP 1.0\n");

        /* Đọc lệnh tiếp theo (LOG 2) và gửi phản hồi 150 tách mảnh */
        if (net_recv_timeout(cfd, rbuf, sizeof rbuf, 1000) > 0) {
            net_send_str(cfd, "150 LOG 2\n2026-11-15 06:00:00 ");
            net_send_str(cfd, "SYS started\n2026-11-15 06:01:00 CMD PUMP\n");
        }
    } else if (ctx->mode == 1) {
        /* Gửi nửa dòng rồi đóng đột ngột */
        net_send_str(cfd, "100 OK 5b1e");
    } else if (ctx->mode == 2) {
        net_send_str(cfd, "200 BAD PASSWORD\n");
    }

    net_close(cfd);
    return NULL;
}

static void test_client_split_lines_and_errors(void) {
    fake_tcp_ctx_t ctx;
    pthread_t th;
    dev_conn_t conn;
    dev_reply_t rep;
    int port;
    int rc;

    dev_conn_init(NULL);
    dev_close(NULL);
    ASSERT_NOT_NULL(dev_strerror_vi(PROTO_BAD_PASSWORD));

    /* Kiểm tra lỗi tham số đầu vào */
    dev_conn_init(&conn);
    ASSERT_EQ_INT(-1, dev_connect(NULL, "127.0.0.1", 7000, "app", "1234", 100, &rep));
    ASSERT_EQ_INT(-1, dev_connect(&conn, "", 7000, "app", "1234", 100, &rep));
    ASSERT_EQ_INT(-1, dev_connect(&conn, "127.0.0.1", 0, "app", "1234", 100, &rep));
    ASSERT_EQ_INT(-1, dev_connect(&conn, "127.0.0.1", 7000, "", "1234", 100, &rep));
    ASSERT_EQ_INT(-1, dev_connect(&conn, "127.0.0.1", 7000, "bad app", "1234", 100, &rep));
    ASSERT_EQ_INT(-1, dev_connect(&conn, "127.0.0.1", 7000, "app", "bad pw", 100, &rep));
    ASSERT_EQ_INT(-1, dev_open_with_token(NULL, "127.0.0.1", 7000, "tok", 100));
    ASSERT_EQ_INT(-1, dev_open_with_token(&conn, "127.0.0.1", 7000, "", 100));
    ASSERT_EQ_INT(NET_ERR_IO, dev_read_reply(&conn, &rep));
    ASSERT_EQ_INT(-1, dev_send_cmd(&conn, "INFO", &rep));

    /* Mode 0: CONNECT tách mảnh + phản hồi 150 LOG 2 tách mảnh */
    ctx.listen_fd = net_tcp_listen("127.0.0.1", 0, 4);
    ASSERT_TRUE(ctx.listen_fd >= 0);
    port = net_bound_port(ctx.listen_fd);
    ctx.mode = 0;

    ASSERT_EQ_INT(0, pthread_create(&th, NULL, fake_tcp_server, &ctx));
    rc = dev_connect(&conn, "127.0.0.1", port, "9f3a1c0b7d2e4a58", "1234", 1000, &rep);
    ASSERT_EQ_INT(PROTO_OK, rc);
    ASSERT_STR_EQ("5b1e77a09c3d4f21", conn.token);
    ASSERT_STR_EQ("P01", conn.device_id);
    ASSERT_EQ_INT(DEV_PUMP, conn.type);
    ASSERT_STR_EQ("1.0", conn.version);

    rc = dev_send_cmd(&conn, "LOG 2", &rep);
    ASSERT_EQ_INT(PROTO_MULTI, rc);
    ASSERT_EQ_INT(1, rep.is_multi);
    ASSERT_STR_EQ("LOG", rep.multi_tag);
    ASSERT_EQ_INT(2, rep.line_count);
    ASSERT_STR_EQ("2026-11-15 06:00:00 SYS started", rep.lines[0]);
    ASSERT_STR_EQ("2026-11-15 06:01:00 CMD PUMP", rep.lines[1]);
    dev_close(&conn);
    pthread_join(th, NULL);

    /* Mode 1: Server đóng kết nối giữa chừng khi chưa hết dòng */
    ctx.mode = 1;
    ASSERT_EQ_INT(0, pthread_create(&th, NULL, fake_tcp_server, &ctx));
    rc = dev_connect(&conn, "127.0.0.1", port, "9f3a1c0b7d2e4a58", "1234", 1000, &rep);
    ASSERT_EQ_INT(-1, rc);
    pthread_join(th, NULL);

    /* Mode 2: Sai mật khẩu -> trả về đúng mã 200 BAD PASSWORD và tự đóng kết nối */
    ctx.mode = 2;
    ASSERT_EQ_INT(0, pthread_create(&th, NULL, fake_tcp_server, &ctx));
    rc = dev_connect(&conn, "127.0.0.1", port, "9f3a1c0b7d2e4a58", "wrong", 1000, &rep);
    ASSERT_EQ_INT(PROTO_BAD_PASSWORD, rc);
    ASSERT_EQ_INT(-1, conn.fd);
    pthread_join(th, NULL);

    net_close(ctx.listen_fd);
}

int main(void) {
    printf("test_scan_client\n");
    RUN_TEST(test_scan_dedup_and_garbage_packets);
    RUN_TEST(test_client_split_lines_and_errors);
    return MT_SUMMARY();
}
