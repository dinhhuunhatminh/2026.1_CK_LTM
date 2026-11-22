#include "server.h"

#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "netutil.h"
#include "proto.h"
#include "simclock.h"
#include "util.h"

#define RECV_CHUNK       1024
#define ACCEPT_BURST     32
#define UDP_BURST        16
#define FULL_LOG_GAP_MS  1000

int server_open(server_t *srv, device_t *dev, const char *bind_ip, int tcp_port, int scan_port) {
    if (!srv || !dev) {
        return -1;
    }
    memset(srv, 0, sizeof *srv);
    srv->dev = dev;
    srv->tcp_fd = -1;
    srv->udp_fd = -1;
    srv->idle_ms = SERVER_IDLE_MS;
    srv->reply = malloc(DEVICE_REPLY_MAX);
    if (!srv->reply) {
        return -1;
    }

    srv->tcp_fd = net_tcp_listen(bind_ip, tcp_port, SERVER_MAX_CLIENTS);
    if (srv->tcp_fd < 0 || net_set_nonblock(srv->tcp_fd, 1) != 0) {
        server_close(srv);
        return -1;
    }
    srv->tcp_port = net_bound_port(srv->tcp_fd);

    if (scan_port >= 0) {
        srv->udp_fd = net_udp_bind(bind_ip, scan_port, 1);
        if (srv->udp_fd < 0 || net_set_nonblock(srv->udp_fd, 1) != 0) {
            server_close(srv);
            return -1;
        }
        srv->scan_port = net_bound_port(srv->udp_fd);
    }
    return 0;
}

void server_close(server_t *srv) {
    if (!srv) {
        return;
    }
    for (int i = 0; i < srv->nclients; i++) {
        net_close(srv->clients[i].fd);
    }
    srv->nclients = 0;
    net_close(srv->tcp_fd);
    net_close(srv->udp_fd);
    srv->tcp_fd = -1;
    srv->udp_fd = -1;
    free(srv->reply);
    srv->reply = NULL;
}

/* Xóa client thứ i bằng cách đưa client cuối vào chỗ trống. */
static void remove_client(server_t *srv, int i) {
    net_close(srv->clients[i].fd);
    srv->nclients--;
    if (i != srv->nclients) {
        srv->clients[i] = srv->clients[srv->nclients];
    }
}

typedef struct {
    server_t        *srv;
    server_client_t *cl;
    int              drop; /* gửi lỗi: phải đóng client */
} feed_ctx_t;

/* Nhận mỗi dòng hoàn chỉnh từ linebuf: xử lý và gửi phản hồi. */
static void on_line(const char *line, size_t len, int status, void *ctx) {
    feed_ctx_t *c = ctx;
    int n;

    (void)len;
    if (c->drop) {
        return;
    }
    if (status != LINEBUF_OK) {
        n = proto_format_reply(c->srv->reply, DEVICE_REPLY_MAX, PROTO_BAD_ARG,
                               status == LINEBUF_TOO_LONG ? "line too long" : "invalid byte");
    } else {
        n = device_handle_line(c->srv->dev, line, simclock_real_ms(),
                               c->srv->reply, DEVICE_REPLY_MAX);
    }
    if (n < 0 || net_send_all(c->cl->fd, c->srv->reply, (size_t)n) != 0) {
        c->drop = 1;
    }
}

/* Đọc dữ liệu của client i. Trả về 1 nếu phải đóng client. */
static int serve_client(server_t *srv, int i) {
    char buf[RECV_CHUNK];
    feed_ctx_t ctx;
    ssize_t n = recv(srv->clients[i].fd, buf, sizeof buf, 0);

    if (n == 0) {
        return 1; /* client đóng kết nối */
    }
    if (n < 0) {
        return !(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
    }
    srv->clients[i].last_ms = simclock_real_ms();
    ctx.srv = srv;
    ctx.cl = &srv->clients[i];
    ctx.drop = 0;
    linebuf_feed(&srv->clients[i].lb, buf, (size_t)n, on_line, &ctx);
    return ctx.drop;
}

/* Trả lời các gói SCAN đang chờ trên socket UDP. */
static void serve_scan(server_t *srv) {
    for (int k = 0; k < UDP_BURST; k++) {
        char buf[256], app_id[64], msg[128];
        struct sockaddr_storage from;
        socklen_t fl = sizeof from;
        ssize_t n = recvfrom(srv->udp_fd, buf, sizeof buf - 1, 0, (struct sockaddr *)&from, &fl);
        int m;

        if (n < 0) {
            break; /* hết gói chờ (EAGAIN) hoặc lỗi */
        }
        buf[n] = '\0';
        if (proto_parse_scan(buf, app_id, sizeof app_id) != 0 ||
            !util_valid_id(app_id, SESSION_ID_MAX)) {
            continue; /* sai định dạng: bỏ qua, không trả lời */
        }
        m = proto_format_device(msg, sizeof msg, srv->dev->id, srv->dev->type, srv->tcp_port);
        if (m > 0) {
            (void)sendto(srv->udp_fd, msg, (size_t)m, 0, (struct sockaddr *)&from, fl);
        }
    }
}

/* Chấp nhận các kết nối đang chờ. */
static void accept_clients(server_t *srv, int64_t now) {
    for (int k = 0; k < ACCEPT_BURST; k++) {
        int fd = net_tcp_accept(srv->tcp_fd, NULL, 0, NULL);
        if (fd < 0) {
            break;
        }
        if (srv->nclients >= SERVER_MAX_CLIENTS || net_set_nonblock(fd, 1) != 0) {
            net_close(fd);
            if (now - srv->last_full_log_ms >= FULL_LOG_GAP_MS) {
                srv->last_full_log_ms = now;
                device_log(srv->dev, now, LOG_SRC_WARN, "connection refused: client limit reached");
            }
            continue;
        }
        srv->clients[srv->nclients].fd = fd;
        linebuf_init(&srv->clients[srv->nclients].lb);
        srv->clients[srv->nclients].last_ms = now;
        srv->nclients++;
    }
}

int server_step(server_t *srv, int timeout_ms) {
    struct pollfd fds[2 + SERVER_MAX_CLIENTS];
    int nfds = 0, tcp_i, udp_i = -1, cl_base;
    int ncl, rc;
    int64_t now;

    if (!srv || srv->tcp_fd < 0) {
        return -1;
    }
    tcp_i = nfds++;
    fds[tcp_i].fd = srv->tcp_fd;
    fds[tcp_i].events = POLLIN;
    if (srv->udp_fd >= 0) {
        udp_i = nfds++;
        fds[udp_i].fd = srv->udp_fd;
        fds[udp_i].events = POLLIN;
    }
    cl_base = nfds;
    ncl = srv->nclients;
    for (int i = 0; i < ncl; i++) {
        fds[nfds].fd = srv->clients[i].fd;
        fds[nfds].events = POLLIN;
        nfds++;
    }

    do {
        rc = poll(fds, (nfds_t)nfds, timeout_ms);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0) {
        return -1;
    }
    now = simclock_real_ms();

    /*
     * Duyệt client từ cuối về đầu: remove_client đưa client cuối vào chỗ trống, mà
     * client cuối đã được xử lý rồi nên không bị bỏ sót. Client mới (accept bên dưới)
     * nằm sau ncl và được xử lý từ vòng sau.
     */
    for (int i = ncl - 1; i >= 0; i--) {
        int close_it = 0;
        if (fds[cl_base + i].revents & (POLLIN | POLLHUP | POLLERR)) {
            close_it = serve_client(srv, i);
        } else if (now - srv->clients[i].last_ms > srv->idle_ms) {
            close_it = 1; /* im lặng quá lâu */
        }
        if (close_it) {
            remove_client(srv, i);
        }
    }
    if (udp_i >= 0 && (fds[udp_i].revents & POLLIN)) {
        serve_scan(srv);
    }
    if (fds[tcp_i].revents & POLLIN) {
        accept_clients(srv, now);
    }
    return 0;
}
