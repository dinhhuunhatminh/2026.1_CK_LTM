#include "netutil.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
static void ensure_net_init(void) {
    static int initialized = 0;
    if (!initialized) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        initialized = 1;
    }
}
static int poll_fd(int fd, short events, int timeout_ms) {
    WSAPOLLFD pfd;
    pfd.fd = (SOCKET)fd;
    pfd.events = (events == POLLOUT) ? POLLWRNORM : POLLRDNORM;
    pfd.revents = 0;
    return WSAPoll(&pfd, 1, timeout_ms);
}
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
static void ensure_net_init(void) {}
static int poll_fd(int fd, short events, int timeout_ms) {
    struct pollfd pfd;
    int rc;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    do {
        rc = poll(&pfd, 1, timeout_ms);
    } while (rc < 0 && errno == EINTR);
    return rc;
}
#endif

void net_close(int fd) {
    if (fd < 0) {
        return;
    }
#ifdef _WIN32
    closesocket((SOCKET)fd);
#else
    close(fd);
#endif
}

int net_set_nonblock(int fd, int nonblock) {
    if (fd < 0) {
        return -1;
    }
#ifdef _WIN32
    {
        u_long mode = nonblock ? 1UL : 0UL;
        return (ioctlsocket((SOCKET)fd, FIONBIO, &mode) == 0) ? 0 : -1;
    }
#else
    {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0) {
            return -1;
        }
        flags = nonblock ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
        return (fcntl(fd, F_SETFL, flags) == 0) ? 0 : -1;
    }
#endif
}

static int fill_sockaddr(const char *ip, int port, struct sockaddr_in *addr) {
    if (addr == NULL || port < 0 || port > 65535) {
        return -1;
    }
    memset(addr, 0, sizeof *addr);
    addr->sin_family = AF_INET;
    addr->sin_port = htons((uint16_t)port);
    if (ip == NULL || ip[0] == '\0' || strcmp(ip, "0.0.0.0") == 0) {
        addr->sin_addr.s_addr = htonl(INADDR_ANY);
        return 0;
    }
    return (inet_pton(AF_INET, ip, &addr->sin_addr) == 1) ? 0 : -1;
}

int net_bound_port(int fd) {
    struct sockaddr_in addr;
    socklen_t len = (socklen_t)sizeof(addr);

    if (fd < 0) {
        return -1;
    }
    memset(&addr, 0, sizeof addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &len) != 0) {
        return -1;
    }
    return (int)ntohs(addr.sin_port);
}

int net_tcp_listen(const char *bind_ip, int port, int backlog) {
    struct sockaddr_in addr;
    int fd;
    int opt = 1;

    ensure_net_init();
    if (fill_sockaddr(bind_ip, port, &addr) != 0) {
        return -1;
    }
    fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                     (const char *)&opt, (socklen_t)sizeof(opt));
    if (bind(fd, (struct sockaddr *)&addr, (socklen_t)sizeof(addr)) != 0 ||
        listen(fd, backlog > 0 ? backlog : 16) != 0) {
        net_close(fd);
        return -1;
    }
    return fd;
}

int net_tcp_accept(int listen_fd, char *peer_ip, size_t ip_sz, int *peer_port) {
    struct sockaddr_in addr;
    socklen_t len = (socklen_t)sizeof(addr);
    int cfd;

    if (listen_fd < 0) {
        return -1;
    }
    memset(&addr, 0, sizeof addr);
    do {
        cfd = (int)accept(listen_fd, (struct sockaddr *)&addr, &len);
    } while (cfd < 0 && errno == EINTR);

    if (cfd < 0) {
        return -1;
    }
    if (peer_ip != NULL && ip_sz > 0) {
        if (inet_ntop(AF_INET, &addr.sin_addr, peer_ip, (socklen_t)ip_sz) == NULL) {
            peer_ip[0] = '\0';
        }
    }
    if (peer_port != NULL) {
        *peer_port = (int)ntohs(addr.sin_port);
    }
    return cfd;
}

int net_tcp_connect(const char *host, int port, int timeout_ms) {
    struct sockaddr_in addr;
    int fd;
    int rc;

    ensure_net_init();
    if (host == NULL || host[0] == '\0' || port <= 0 || port > 65535 ||
        fill_sockaddr(host, port, &addr) != 0) {
        return -1;
    }
    fd = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    (void)net_set_nonblock(fd, 1);

    rc = connect(fd, (struct sockaddr *)&addr, (socklen_t)sizeof(addr));
    if (rc != 0) {
        int wait_ms = (timeout_ms > 0) ? timeout_ms : 3000;
        int so_err = 0;
        socklen_t elen = (socklen_t)sizeof(so_err);

        if (poll_fd(fd, POLLOUT, wait_ms) <= 0 ||
            getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&so_err, &elen) != 0 ||
            so_err != 0) {
            net_close(fd);
            return -1;
        }
    }
    (void)net_set_nonblock(fd, 0);
    return fd;
}

int net_udp_bind(const char *bind_ip, int port, int reuse) {
    struct sockaddr_in addr;
    int fd;

    ensure_net_init();
    if (fill_sockaddr(bind_ip, port, &addr) != 0) {
        return -1;
    }
    fd = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }
    if (reuse) {
        int opt = 1;
        (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                         (const char *)&opt, (socklen_t)sizeof(opt));
#ifdef SO_REUSEPORT
        (void)setsockopt(fd, SOL_SOCKET, SO_REUSEPORT,
                         (const char *)&opt, (socklen_t)sizeof(opt));
#endif
    }
    if (bind(fd, (struct sockaddr *)&addr, (socklen_t)sizeof(addr)) != 0) {
        net_close(fd);
        return -1;
    }
    return fd;
}

int net_udp_open_broadcast(void) {
    int fd;
    int opt = 1;

    ensure_net_init();
    fd = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }
    (void)setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
                     (const char *)&opt, (socklen_t)sizeof(opt));
    return fd;
}

int net_send_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t sent_total = 0;

    if (fd < 0 || (buf == NULL && len > 0)) {
        return -1;
    }
    while (sent_total < len) {
#ifdef _WIN32
        int n = send((SOCKET)fd, p + sent_total, (int)(len - sent_total), 0);
#else
        ssize_t n = send(fd, p + sent_total, len - sent_total, MSG_NOSIGNAL);
#endif
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        sent_total += (size_t)n;
    }
    return 0;
}

int net_send_str(int fd, const char *s) {
    if (s == NULL) {
        return -1;
    }
    return net_send_all(fd, s, strlen(s));
}

int net_recv_timeout(int fd, void *buf, size_t cap, int timeout_ms) {
    if (fd < 0 || buf == NULL || cap == 0) {
        return NET_ERR_IO;
    }
    if (timeout_ms >= 0) {
        int prc = poll_fd(fd, POLLIN, timeout_ms);
        if (prc == 0) {
            return NET_ERR_TIMEOUT;
        }
        if (prc < 0) {
            return NET_ERR_IO;
        }
    }
    for (;;) {
#ifdef _WIN32
        int n = recv((SOCKET)fd, (char *)buf, (int)cap, 0);
#else
        ssize_t n = recv(fd, buf, cap, 0);
#endif
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return NET_ERR_IO;
        }
        return (int)n;
    }
}
