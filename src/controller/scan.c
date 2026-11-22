#include "scan.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#endif

#include "netutil.h"
#include "simclock.h"
#include "util.h"

int scan_send(const char *target_ip, int scan_port, const char *app_id) {
    const char *ip = (target_ip && target_ip[0] != '\0') ? target_ip : SCAN_DEFAULT_IP;
    int port = (scan_port > 0) ? scan_port : SCAN_DEFAULT_PORT;
    struct sockaddr_in dst;
    char msg[128];
    int msg_len;
    int fd;
    int sent;

    if (port > 65535 || app_id == NULL || !util_valid_id(app_id, SCAN_ID_MAX)) {
        return -1;
    }
    memset(&dst, 0, sizeof dst);
    dst.sin_family = AF_INET;
    dst.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &dst.sin_addr) != 1) {
        return -1;
    }
    msg_len = proto_format_scan(msg, sizeof msg, app_id);
    if (msg_len <= 0) {
        return -1;
    }
    fd = net_udp_open_broadcast();
    if (fd < 0) {
        return -1;
    }
#ifdef _WIN32
    sent = sendto((SOCKET)fd, msg, msg_len, 0,
                  (struct sockaddr *)&dst, (int)sizeof(dst));
#else
    sent = (int)sendto(fd, msg, (size_t)msg_len, 0,
                       (struct sockaddr *)&dst, (socklen_t)sizeof(dst));
#endif
    if (sent != msg_len) {
        net_close(fd);
        return -1;
    }
    return fd;
}

static int already_found(const scan_device_t *results, int count, const char *id) {
    for (int i = 0; i < count; i++) {
        if (strcmp(results[i].device_id, id) == 0) {
            return 1;
        }
    }
    return 0;
}

static int wait_readable(int fd, int wait_ms) {
#ifdef _WIN32
    WSAPOLLFD pfd;
    pfd.fd = (SOCKET)fd;
    pfd.events = POLLRDNORM;
    pfd.revents = 0;
    return WSAPoll(&pfd, 1, wait_ms);
#else
    struct pollfd pfd;
    int rc;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    do {
        rc = poll(&pfd, 1, wait_ms);
    } while (rc < 0 && errno == EINTR);
    return rc;
#endif
}

int scan_collect(int udp_fd, int timeout_ms, scan_device_t *results, int max_results) {
    int total_wait = (timeout_ms >= 0) ? timeout_ms : SCAN_DEFAULT_TIMEOUT_MS;
    int64_t deadline;
    int count = 0;

    if (udp_fd < 0 || results == NULL || max_results <= 0) {
        return -1;
    }
    deadline = simclock_real_ms() + total_wait;

    while (count < max_results) {
        int64_t now = simclock_real_ms();
        int rem = (int)(deadline - now);
        struct sockaddr_in from;
        socklen_t flen = (socklen_t)sizeof(from);
        char buf[256];
        char dev_id[SCAN_ID_MAX + 1];
        dev_type_t dt = DEV_NONE;
        int tcp_port = 0;
        int prc;
        int n;

        if (rem < 0) {
            rem = 0;
        }
        prc = wait_readable(udp_fd, rem);
        if (prc <= 0) {
            break;
        }
        memset(&from, 0, sizeof from);
#ifdef _WIN32
        n = recvfrom((SOCKET)udp_fd, buf, (int)(sizeof(buf) - 1), 0,
                     (struct sockaddr *)&from, &flen);
#else
        n = (int)recvfrom(udp_fd, buf, sizeof(buf) - 1, 0,
                          (struct sockaddr *)&from, &flen);
#endif
        if (n <= 0) {
            break;
        }
        buf[n] = '\0';

        if (proto_parse_device(buf, dev_id, sizeof dev_id, &dt, &tcp_port) != 0 ||
            !util_valid_id(dev_id, SCAN_ID_MAX)) {
            if (total_wait == 0) {
                continue;
            }
            continue; /* bỏ qua gói sai định dạng */
        }
        if (already_found(results, count, dev_id)) {
            continue; /* loại trùng device_id */
        }

        memset(&results[count], 0, sizeof results[count]);
        util_str_copy(results[count].device_id, sizeof results[count].device_id, dev_id);
        results[count].type = dt;
        results[count].tcp_port = tcp_port;
        if (inet_ntop(AF_INET, &from.sin_addr,
                      results[count].host, (socklen_t)sizeof(results[count].host)) == NULL) {
            util_str_copy(results[count].host, sizeof results[count].host, "127.0.0.1");
        }
        count++;
    }
    return count;
}

int scan_devices(const char *target_ip, int scan_port, const char *app_id,
                 int timeout_ms, scan_device_t *results, int max_results) {
    int fd;
    int count;

    if (results == NULL || max_results <= 0) {
        return -1;
    }
    fd = scan_send(target_ip, scan_port, app_id);
    if (fd < 0) {
        return -1;
    }
    count = scan_collect(fd, timeout_ms, results, max_results);
    net_close(fd);
    return count;
}
