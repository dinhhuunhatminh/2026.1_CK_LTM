/*
 * device.h - lõi của một thiết bị mô phỏng: trạng thái và xử lý từng dòng lệnh.
 *
 * Module này không dùng socket: nhận một dòng lệnh dạng văn bản, trả về phản hồi
 * dạng văn bản. Phần mạng nằm ở server.c. Nhờ vậy toàn bộ logic giao thức
 * (xác thực, token, lệnh chung) được unit test không cần mạng.
 *
 * Lệnh chung cho mọi loại thiết bị (DESIGN.md mục 3.4, 3.6):
 *   CONNECT <app_id> <password>   -> 100 OK <token> <id> <type> <version>
 *   <token> INFO                  -> 100 OK id=.. type=.. version=.. uptime=..
 *   <token> STATUS                -> 100 OK power=ON|OFF
 *   <token> PARAMS                -> 100 OK k1=v1 k2=v2 ...
 *   <token> POWER ON|OFF          -> 100 OK
 *   <token> PASSWD <cũ> <mới>     -> 100 OK  (thu hồi mọi token khác, lưu file)
 *   <token> LOG [n]               -> 150 LOG <N> + N dòng
 *
 * Lệnh riêng của từng loại (SET, DATA, PUMP, ...) sẽ được bổ sung ở các commit
 * dev_sensor, dev_pump, dev_fert, dev_light; hiện trả 203 (chưa hỗ trợ) hoặc 204
 * (thiết bị đang tắt).
 */
#ifndef DEVICE_H
#define DEVICE_H

#include <stddef.h>
#include <stdint.h>

#include "log.h"
#include "proto.h"
#include "session.h"
#include "simclock.h"
#include "store.h"

#define DEVICE_VERSION        "1.0"
#define DEVICE_ID_MAX         16
#define DEVICE_PATH_MAX       256
#define DEVICE_PASS_BUF       128    /* đủ chứa mật khẩu sai quy cách để báo lỗi rõ ràng */
#define DEVICE_REPLY_MAX      65536  /* bộ đệm phản hồi, đủ cho LOG tối đa */
#define DEVICE_LOG_MAX_LINES  100    /* LOG n bị giới hạn ở mức này */

/* Cấu hình khởi tạo, thường lấy từ tham số dòng lệnh. */
typedef struct {
    dev_type_t type;
    char       id[DEVICE_ID_MAX + 1];
    char       password[DEVICE_PASS_BUF]; /* chỉ dùng khi chưa có file trạng thái */
    char       state_dir[DEVICE_PATH_MAX]; /* rỗng = không lưu trạng thái */
    char       log_dir[DEVICE_PATH_MAX];
    double     timescale;                  /* hệ số tăng tốc đồng hồ giả lập, > 0 */
} device_config_t;

typedef struct {
    dev_type_t type;
    char       id[DEVICE_ID_MAX + 1];
    store_t    store;   /* mật khẩu + tham số */
    session_t  sess;    /* ứng dụng đã xác thực */
    logger_t   log;
    simclock_t clock;
    int        power;   /* 1 = bật, 0 = tắt */
    int64_t    start_sim_sec;
    char       state_dir[DEVICE_PATH_MAX];
} device_t;

/*
 * Khởi tạo thiết bị (DESIGN.md mục 4.1):
 *   - nếu <state_dir>/<id>.state tồn tại thì dùng giá trị trong file (mật khẩu đã
 *     đổi, tham số đã chỉnh) và bỏ qua cfg->password;
 *   - ngược lại dùng cfg->password (phải hợp lệ) và lưu trạng thái ban đầu.
 * Tạo thư mục log nếu chưa có và ghi dòng SYS khởi động.
 * Trả về 0 nếu thành công; -1 nếu tham số sai; -2 nếu lỗi trạng thái/mật khẩu/log.
 * Khi lỗi, err (nếu khác NULL) chứa thông báo cho người dùng.
 */
int device_init(device_t *d, const device_config_t *cfg, int64_t real_now_ms,
                char *err, size_t err_sz);

/*
 * Xử lý một dòng lệnh (không gồm '\n') và dựng phản hồi vào out (kết thúc '\n';
 * phản hồi nhiều dòng thì mỗi dòng kết thúc '\n').
 * Trả về số byte của phản hồi, hoặc -1 nếu tham số sai / bộ đệm không đủ.
 * Luôn có phản hồi cho mọi dòng, kể cả dòng sai cú pháp.
 */
int device_handle_line(device_t *d, const char *line, int64_t real_now_ms,
                       char *out, size_t out_sz);

/* Ghi một dòng log với giờ giả lập ứng với real_now_ms. msg không được là định dạng printf. */
void device_log(device_t *d, int64_t real_now_ms, log_source_t src, const char *msg);

#endif /* DEVICE_H */
