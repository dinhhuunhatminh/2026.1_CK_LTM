/*
 * client.h - kết nối TCP từ controller tới thiết bị, xác thực, gửi lệnh và đọc phản hồi.
 *
 * Theo DESIGN.md mục 3.4, 3.5, 5.3:
 *   - dev_connect() gửi "CONNECT <app_id> <password>\n", đọc phản hồi
 *     "100 OK <token> <device_id> <type> <version>" và lưu token vào kết nối.
 *   - dev_send_cmd() tự động ghép "<token> <cmd_line>\n", gửi qua TCP và đọc
 *     phản hồi bằng dev_read_reply().
 *   - dev_read_reply() dùng linebuf để xử lý truyền dòng TCP, hỗ trợ cả phản hồi
 *     1 dòng và phản hồi nhiều dòng "150 <TAG> <N>\n" kèm N dòng dữ liệu.
 *   - Thời gian chờ mặc định mỗi lệnh: 3000 ms.
 */
#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>

#include "linebuf.h"
#include "proto.h"

#define CLIENT_DEFAULT_TIMEOUT_MS 3000
#define CLIENT_HOST_MAX           64
#define CLIENT_ID_MAX             16
#define CLIENT_VER_MAX            16
#define CLIENT_MAX_MULTI_LINES    100

typedef struct {
    int        fd;
    char       host[CLIENT_HOST_MAX + 1];
    int        port;
    int        timeout_ms;
    char       token[PROTO_MAX_TOKEN];
    char       device_id[CLIENT_ID_MAX + 1];
    dev_type_t type;
    char       version[CLIENT_VER_MAX + 1];
    linebuf_t  lb;
} dev_conn_t;

typedef struct {
    int  code;                                             /* mã phản hồi: 100, 150, 200... */
    int  is_multi;                                         /* 1 nếu code == 150 */
    char multi_tag[PROTO_MAX_TOKEN];                       /* nhãn sau 150, vd "LOG" */
    char detail[PROTO_MAX_LINE + 1];                       /* phần dữ liệu của phản hồi 1 dòng */
    int  line_count;                                       /* số dòng dữ liệu nhận được (0..100) */
    char lines[CLIENT_MAX_MULTI_LINES][PROTO_MAX_LINE + 1];/* các dòng dữ liệu của phản hồi 150 */
} dev_reply_t;

/* Khởi tạo cấu trúc kết nối ở trạng thái chưa mở (fd = -1). */
void dev_conn_init(dev_conn_t *conn);

/* Đóng kết nối TCP với thiết bị (an toàn khi gọi nhiều lần). */
void dev_close(dev_conn_t *conn);

/*
 * Mở kết nối TCP tới host:port với một token đã lưu sẵn (không gửi CONNECT).
 * Trả về 0 nếu mở socket thành công, -1 nếu lỗi.
 */
int dev_open_with_token(dev_conn_t *conn, const char *host, int port,
                        const char *token, int timeout_ms);

/*
 * Đọc một phản hồi hoàn chỉnh (1 dòng hoặc 150 kèm N dòng) từ kết nối conn.
 * Trả về 0 nếu đọc được phản hồi hợp lệ vào *reply,
 * NET_ERR_TIMEOUT (-2) nếu hết thời gian chờ, hoặc NET_ERR_IO (-1) nếu lỗi/mất kết nối.
 */
int dev_read_reply(dev_conn_t *conn, dev_reply_t *reply);

/*
 * Kết nối TCP tới host:port và xác thực bằng "CONNECT <app_id> <password>".
 *   - Nếu đúng mật khẩu (100 OK): lưu token, device_id, type, version vào *conn,
 *     ghi phản hồi vào *reply_out (nếu khác NULL) và trả về PROTO_OK (100).
 *   - Nếu thiết bị từ chối (200 BAD PASSWORD, 205 TOO MANY APPS, 206 LOCKED...):
 *     đóng socket, ghi phản hồi vào *reply_out và trả về đúng mã lỗi của giao thức.
 *   - Nếu lỗi mạng/timeout/tham số sai: đóng socket và trả về -1.
 */
int dev_connect(dev_conn_t *conn, const char *host, int port,
                const char *app_id, const char *password,
                int timeout_ms, dev_reply_t *reply_out);

/*
 * Gửi một lệnh đã xác thực (vd: "INFO", "SET HMIN 30", "LOG 5") tới thiết bị.
 * Hàm tự động ghép "<conn->token> <cmd_line>\n" rồi đọc phản hồi vào *reply.
 * Trả về mã phản hồi (100, 150, 201, 202, 203, 204...) nếu nhận được phản hồi hợp lệ,
 * hoặc -1 nếu lỗi mạng / chưa có token / hết thời gian chờ.
 */
int dev_send_cmd(dev_conn_t *conn, const char *cmd_line, dev_reply_t *reply);

/* Trả về thông báo tiếng Việt thân thiện ứng với mã phản hồi code (dùng proto_code_vi). */
const char *dev_strerror_vi(int code);

#endif /* CLIENT_H */
