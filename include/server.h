/*
 * server.h - vòng lặp vào/ra socket của thiết bị bằng poll() (DESIGN.md mục 4.2).
 *
 * Một luồng duy nhất theo dõi cùng lúc:
 *   - socket UDP quét thiết bị: nhận "SCAN <app_id>", trả "DEVICE <id> <type> <tcp_port>";
 *   - socket TCP lắng nghe: chấp nhận kết nối (không chặn);
 *   - các socket TCP của client (tối đa SERVER_MAX_CLIENTS): đọc dữ liệu, ghép dòng
 *     bằng linebuf, chuyển từng dòng cho device_handle_line, gửi phản hồi.
 *
 * Client im lặng quá idle_ms bị đóng. Client không đọc phản hồi làm đầy bộ đệm gửi
 * cũng bị đóng (net_send_all lỗi) để không làm treo cả vòng lặp.
 *
 * Cách dùng:
 *     server_t srv;
 *     server_open(&srv, &dev, "0.0.0.0", 7002, 9999);
 *     while (running) server_step(&srv, 1000);
 *     server_close(&srv);
 */
#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>

#include "device.h"
#include "linebuf.h"

#define SERVER_MAX_CLIENTS 64
#define SERVER_IDLE_MS     120000 /* 120 giây */
#define SERVER_SCAN_PORT   9999

typedef struct {
    int       fd;
    linebuf_t lb;
    int64_t   last_ms; /* lần cuối nhận dữ liệu (ms thật) */
} server_client_t;

typedef struct {
    device_t       *dev;
    int             tcp_fd;
    int             udp_fd;       /* -1 nếu không bật quét UDP */
    int             tcp_port;     /* cổng TCP thực tế đang nghe */
    int             scan_port;    /* cổng UDP thực tế, 0 nếu không bật quét */
    int             idle_ms;      /* mặc định SERVER_IDLE_MS */
    int             nclients;
    int64_t         last_full_log_ms;
    char           *reply;        /* bộ đệm phản hồi DEVICE_REPLY_MAX byte */
    server_client_t clients[SERVER_MAX_CLIENTS];
} server_t;

/*
 * Mở các socket.
 *   bind_ip   : địa chỉ nghe (NULL hoặc "0.0.0.0" = mọi giao diện).
 *   tcp_port  : cổng TCP (0 = hệ điều hành chọn, đọc lại ở srv->tcp_port).
 *   scan_port : cổng UDP quét (thường SERVER_SCAN_PORT); 0 = hệ điều hành chọn;
 *               số âm = không bật quét UDP.
 * Trả về 0 nếu thành công, -1 nếu lỗi (đã dọn dẹp, không cần server_close).
 */
int server_open(server_t *srv, device_t *dev, const char *bind_ip, int tcp_port, int scan_port);

/*
 * Chạy một vòng: chờ sự kiện tối đa timeout_ms (ms) rồi xử lý mọi socket sẵn sàng.
 * Trả về 0 nếu bình thường (kể cả hết thời gian chờ), -1 nếu lỗi nghiêm trọng.
 */
int server_step(server_t *srv, int timeout_ms);

/* Đóng mọi socket và giải phóng bộ nhớ. Gọi nhiều lần cũng an toàn. */
void server_close(server_t *srv);

#endif /* SERVER_H */
