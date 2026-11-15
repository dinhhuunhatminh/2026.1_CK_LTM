/*
 * log.h - ghi và đọc lại file log hoạt động trên từng thiết bị.
 *
 * Quy ước (DESIGN.md mục 4.4):
 *   Mỗi dòng có dạng:
 *     YYYY-MM-DD HH:MM:SS <NGUỒN> <nội dung>
 *   với NGUỒN thuộc: AUTH, CMD, AUTO, SYS, WARN.
 */
#ifndef LOG_H
#define LOG_H

#include <stddef.h>
#include <stdint.h>

#define LOG_MAX_PATH 256
#define LOG_MAX_LINE 512

typedef enum {
    LOG_SRC_AUTH = 0, /* kết nối, sai mật khẩu, đổi mật khẩu */
    LOG_SRC_CMD,      /* lệnh từ controller */
    LOG_SRC_AUTO,     /* hành động tự động (tưới, bón, đèn, đo) */
    LOG_SRC_SYS,      /* khởi động, tắt thiết bị */
    LOG_SRC_WARN      /* cảnh báo (vd: không liên lạc được cảm biến) */
} log_source_t;

typedef struct {
    char path[LOG_MAX_PATH];
} logger_t;

/* Trả về chuỗi tên nguồn log ("AUTH", "CMD", "AUTO", "SYS", "WARN"). */
const char *log_source_str(log_source_t src);

/* Khởi tạo logger ghi vào đường dẫn file path. Trả về 0 hoặc -1. */
int log_init(logger_t *lg, const char *path);

/*
 * Ghi một dòng log có mốc thời gian sim_sec (giây) vào cuối file.
 * Trả về 0 nếu thành công, -1 nếu lỗi.
 */
int log_write(const logger_t *lg, int64_t sim_sec,
              log_source_t src, const char *fmt, ...);

/*
 * Đọc lại tối đa max_lines dòng cuối cùng của file log vào mảng lines.
 * Mỗi phần tử lines[i] đã được cắt bỏ '\r', '\n' ở cuối.
 * Trả về số dòng đọc được (0..max_lines), hoặc -1 nếu tham số không hợp lệ.
 */
int log_tail(const logger_t *lg, char lines[][LOG_MAX_LINE], int max_lines);

#endif /* LOG_H */
