/*
 * netutil.h - các hàm tiện ích mạng TCP/UDP: tạo socket, gửi đủ byte (send_all),
 *             nhận dữ liệu có timeout và lấy cổng đang lắng nghe.
 *
 * Thiết kế theo DESIGN.md mục 3.2, 3.3, 4.2, 5.3.
 */
#ifndef NETUTIL_H
#define NETUTIL_H

#include <stddef.h>

#define NET_ERR_IO      (-1)
#define NET_ERR_TIMEOUT (-2)

/* Đóng socket an toàn (bỏ qua nếu fd < 0). */
void net_close(int fd);

/* Bật (nonblock = 1) hoặc tắt (nonblock = 0) chế độ không chặn cho socket. */
int net_set_nonblock(int fd, int nonblock);

/* Lấy số cổng cục bộ mà socket fd đang gắn (hữu ích khi bind với port = 0). */
int net_bound_port(int fd);

/*
 * Tạo socket TCP server, bật SO_REUSEADDR, bind tới bind_ip:port và listen.
 *   bind_ip : địa chỉ IPv4 (vd "127.0.0.1"), hoặc NULL/"0.0.0.0" để nghe mọi giao diện.
 *   port    : 0..65535 (0 để hệ điều hành tự cấp cổng trống).
 *   backlog : độ dài hàng đợi kết nối (nếu <= 0 sẽ dùng mặc định 16).
 * Trả về fd >= 0 nếu thành công, hoặc -1 nếu lỗi.
 */
int net_tcp_listen(const char *bind_ip, int port, int backlog);

/*
 * Chấp nhận một kết nối từ listen_fd.
 *   peer_ip   : bộ đệm nhận chuỗi IP của client (có thể NULL).
 *   ip_sz     : kích thước peer_ip.
 *   peer_port : nhận cổng của client (có thể NULL).
 * Trả về fd >= 0 nếu thành công, -1 nếu lỗi.
 */
int net_tcp_accept(int listen_fd, char *peer_ip, size_t ip_sz, int *peer_port);

/*
 * Kết nối TCP tới host:port với thời gian chờ tối đa timeout_ms (mili-giây).
 * Nếu timeout_ms <= 0 sẽ dùng mặc định 3000 ms.
 * Trả về fd >= 0 nếu thành công, hoặc -1 nếu lỗi / hết thời gian chờ.
 */
int net_tcp_connect(const char *host, int port, int timeout_ms);

/*
 * Tạo socket UDP và bind tới bind_ip:port.
 * Nếu reuse != 0 sẽ bật SO_REUSEADDR (và SO_REUSEPORT nếu có) để nhiều thiết bị
 * cùng máy có thể lắng nghe cổng quét 9999.
 * Trả về fd >= 0 hoặc -1.
 */
int net_udp_bind(const char *bind_ip, int port, int reuse);

/* Tạo socket UDP cho phép gửi quảng bá (SO_BROADCAST). Trả về fd >= 0 hoặc -1. */
int net_udp_open_broadcast(void);

/*
 * Gửi toàn bộ len byte trong buf qua socket TCP fd.
 * Tự động lặp nếu send() chỉ gửi được một phần hoặc bị ngắt bởi EINTR;
 * dùng MSG_NOSIGNAL để tránh tín hiệu SIGPIPE khi phía bên kia đóng kết nối.
 * Trả về 0 nếu gửi đủ len byte, -1 nếu lỗi.
 */
int net_send_all(int fd, const void *buf, size_t len);

/* Gửi một chuỗi kết thúc bằng '\0' qua socket TCP fd. Trả về 0 hoặc -1. */
int net_send_str(int fd, const char *s);

/*
 * Nhận dữ liệu từ socket fd với thời gian chờ timeout_ms (mili-giây):
 *   timeout_ms < 0 : chờ vô hạn
 *   timeout_ms == 0: kiểm tra tức thời không chờ
 *   timeout_ms > 0 : chờ tối đa timeout_ms mili-giây
 * Trả về:
 *   > 0             : số byte đọc được
 *   0               : phía bên kia đã đóng kết nối (EOF)
 *   NET_ERR_TIMEOUT : hết thời gian chờ (-2)
 *   NET_ERR_IO      : lỗi (-1)
 */
int net_recv_timeout(int fd, void *buf, size_t cap, int timeout_ms);

#endif /* NETUTIL_H */
