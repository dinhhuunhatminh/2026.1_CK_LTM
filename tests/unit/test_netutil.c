/* Unit test cho netutil: tạo socket TCP/UDP, send_all và đọc có timeout trên loopback. */
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "linebuf.h"
#include "minitest.h"
#include "netutil.h"

static void on_line_copy(const char *line, size_t len, int status, void *ctx) {
    char *dst = (char *)ctx;
    if (status == LINEBUF_OK && len < 256) {
        strcpy(dst, line);
    }
}

static void test_invalid_arguments(void) {
    char buf[32];

    net_close(-1);
    ASSERT_EQ_INT(-1, net_set_nonblock(-1, 1));
    ASSERT_EQ_INT(-1, net_bound_port(-1));
    ASSERT_EQ_INT(-1, net_bound_port(99999));

    ASSERT_EQ_INT(-1, net_tcp_listen("999.999.999.999", 7000, 5));
    ASSERT_EQ_INT(-1, net_tcp_listen("127.0.0.1", -1, 5));
    ASSERT_EQ_INT(-1, net_tcp_listen("127.0.0.1", 70000, 5));
    ASSERT_EQ_INT(-1, net_tcp_accept(-1, NULL, 0, NULL));
    ASSERT_EQ_INT(-1, net_tcp_accept(99999, NULL, 0, NULL));

    ASSERT_EQ_INT(-1, net_tcp_connect(NULL, 7000, 100));
    ASSERT_EQ_INT(-1, net_tcp_connect("", 7000, 100));
    ASSERT_EQ_INT(-1, net_tcp_connect("127.0.0.1", 0, 100));
    ASSERT_EQ_INT(-1, net_tcp_connect("127.0.0.1", 70000, 100));
    ASSERT_EQ_INT(-1, net_tcp_connect("bad_ip", 7000, 100));

    ASSERT_EQ_INT(-1, net_udp_bind("bad_ip", 9999, 1));
    ASSERT_EQ_INT(-1, net_udp_bind("127.0.0.1", -1, 1));

    ASSERT_EQ_INT(-1, net_send_all(-1, "abc", 3));
    ASSERT_EQ_INT(-1, net_send_all(1, NULL, 3));
    ASSERT_EQ_INT(0, net_send_all(1, NULL, 0));
    ASSERT_EQ_INT(-1, net_send_all(99999, "abc", 3));
    ASSERT_EQ_INT(-1, net_send_str(1, NULL));

    ASSERT_EQ_INT(NET_ERR_IO, net_recv_timeout(-1, buf, sizeof buf, 10));
    ASSERT_EQ_INT(NET_ERR_IO, net_recv_timeout(1, NULL, sizeof buf, 10));
    ASSERT_EQ_INT(NET_ERR_IO, net_recv_timeout(1, buf, 0, 10));
    ASSERT_EQ_INT(NET_ERR_IO, net_recv_timeout(99999, buf, sizeof buf, -1));
}

static void test_tcp_loopback_send_all_and_recv_timeout(void) {
    int lfd;
    int lfd_any;
    int port;
    int cfd;
    int sfd;
    char peer_ip[64];
    int peer_port = 0;
    char rbuf[128];
    char got_line[256] = {0};
    linebuf_t lb;
    int n;

    /* Kiểm tra bind với NULL ("0.0.0.0") và backlog mặc định */
    lfd_any = net_tcp_listen(NULL, 0, 0);
    ASSERT_TRUE(lfd_any >= 0);
    net_close(lfd_any);

    lfd = net_tcp_listen("127.0.0.1", 0, 8);
    ASSERT_TRUE(lfd >= 0);
    port = net_bound_port(lfd);
    ASSERT_TRUE(port > 0 && port <= 65535);

    cfd = net_tcp_connect("127.0.0.1", port, 1000);
    ASSERT_TRUE(cfd >= 0);

    sfd = net_tcp_accept(lfd, peer_ip, sizeof peer_ip, &peer_port);
    ASSERT_TRUE(sfd >= 0);
    ASSERT_STR_EQ("127.0.0.1", peer_ip);
    ASSERT_TRUE(peer_port > 0);

    /* Chưa gửi gì -> đọc với timeout ngắn phải trả về NET_ERR_TIMEOUT */
    ASSERT_EQ_INT(NET_ERR_TIMEOUT, net_recv_timeout(sfd, rbuf, sizeof rbuf, 20));

    /* Gửi chuỗi lệnh qua net_send_str / net_send_all và ghép bằng linebuf */
    ASSERT_EQ_INT(0, net_send_str(cfd, "CONNECT 9f3a1c0b7d2e4a58 1234\n"));
    linebuf_init(&lb);
    n = net_recv_timeout(sfd, rbuf, sizeof rbuf, -1);
    ASSERT_TRUE(n > 0);
    linebuf_feed(&lb, rbuf, (size_t)n, on_line_copy, got_line);
    ASSERT_STR_EQ("CONNECT 9f3a1c0b7d2e4a58 1234", got_line);

    /* Gửi dữ liệu lớn hơn để kiểm tra vòng lặp send_all */
    {
        char big_tx[4096];
        char big_rx[4096];
        size_t total = 0;

        memset(big_tx, 'Z', sizeof big_tx);
        ASSERT_EQ_INT(0, net_send_all(sfd, big_tx, sizeof big_tx));
        while (total < sizeof big_rx) {
            n = net_recv_timeout(cfd, big_rx + total, sizeof(big_rx) - total, 1000);
            ASSERT_TRUE(n > 0);
            total += (size_t)n;
        }
        ASSERT_EQ_INT(0, memcmp(big_tx, big_rx, sizeof big_tx));
    }

    /* Đóng phía client -> phía server đọc được 0 (EOF) */
    net_close(cfd);
    n = net_recv_timeout(sfd, rbuf, sizeof rbuf, 1000);
    ASSERT_EQ_INT(0, n);

    net_close(sfd);
    net_close(lfd);

    /* Kết nối tới cổng đã đóng -> phải báo lỗi -1 */
    ASSERT_EQ_INT(-1, net_tcp_connect("127.0.0.1", port, 100));
}

static void test_udp_loopback_and_broadcast_socket(void) {
    int u1;
    int u2;
    int bfd;
    int port;
    struct sockaddr_in dst;
    const char *msg = "SCAN 9f3a1c0b7d2e4a58\n";
    char rbuf[64];
    int n;

    u1 = net_udp_bind("127.0.0.1", 0, 1);
    ASSERT_TRUE(u1 >= 0);
    port = net_bound_port(u1);
    ASSERT_TRUE(port > 0);

    /* Kiểm tra bind thêm socket UDP với reuse trên cùng cổng */
    u2 = net_udp_bind("127.0.0.1", port, 1);
    if (u2 >= 0) {
        net_close(u2);
    }

    bfd = net_udp_open_broadcast();
    ASSERT_TRUE(bfd >= 0);

    memset(&dst, 0, sizeof dst);
    dst.sin_family = AF_INET;
    dst.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, "127.0.0.1", &dst.sin_addr);

    n = (int)sendto(bfd, msg, (int)strlen(msg), 0,
                    (struct sockaddr *)&dst, (int)sizeof(dst));
    ASSERT_EQ_INT((int)strlen(msg), n);

    n = net_recv_timeout(u1, rbuf, sizeof(rbuf) - 1, 1000);
    ASSERT_EQ_INT((int)strlen(msg), n);
    rbuf[n] = '\0';
    ASSERT_STR_EQ(msg, rbuf);

    net_close(bfd);
    net_close(u1);
}

int main(void) {
    printf("test_netutil\n");
    RUN_TEST(test_invalid_arguments);
    RUN_TEST(test_tcp_loopback_send_all_and_recv_timeout);
    RUN_TEST(test_udp_loopback_and_broadcast_socket);
    return MT_SUMMARY();
}
