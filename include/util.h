/*
 * util.h - các hàm tiện ích dùng chung: sinh hex ngẫu nhiên, xử lý chuỗi,
 *          chuyển đổi số và giờ HH:MM có kiểm tra lỗi.
 */
#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

/*
 * Sinh chuỗi gồm hex_len ký tự hex thường ('0'..'9', 'a'..'f') từ nguồn ngẫu nhiên
 * hệ thống (/dev/urandom). Bộ đệm buf phải có kích thước ít nhất hex_len + 1 byte.
 * Trả về 0 nếu thành công, -1 nếu lỗi.
 */
int util_random_hex(char *buf, size_t hex_len);

/*
 * Giống util_random_hex nhưng đọc byte ngẫu nhiên từ đường dẫn file bất kỳ
 * (dùng để kiểm thử các nhánh lỗi đọc file).
 */
int util_random_hex_from_file(const char *path, char *buf, size_t hex_len);

/* Cắt khoảng trắng (' ', '\t', '\r', '\n') ở đầu và cuối chuỗi ngay tại chỗ. */
char *util_trim(char *s);

/* Sao chép chuỗi an toàn vào dst (kích thước dst_sz). Trả về 0 nếu vừa, -1 nếu bị cắt/lỗi. */
int util_str_copy(char *dst, size_t dst_sz, const char *src);

/*
 * Kiểm tra ID hợp lệ theo DESIGN.md mục 3.1:
 *   độ dài 1..max_len, chỉ gồm [A-Za-z0-9_-].
 * Trả về 1 nếu hợp lệ, 0 nếu không hợp lệ.
 */
int util_valid_id(const char *s, size_t max_len);

/* Kiểm tra chuỗi có phải đúng hex_len ký tự thập lục phân [0-9a-fA-F] hay không. */
int util_valid_hex(const char *s, size_t hex_len);

/*
 * Đổi chuỗi sang số nguyên trong đoạn [min_val, max_val].
 * Trả về 0 nếu hợp lệ (ghi kết quả vào *out), -1 nếu sai định dạng hoặc ngoài khoảng.
 */
int util_parse_int(const char *s, int min_val, int max_val, int *out);

/*
 * Đổi chuỗi sang số thực double trong đoạn [min_val, max_val].
 * Trả về 0 nếu hợp lệ (ghi kết quả vào *out), -1 nếu sai định dạng, NaN/Inf hoặc ngoài khoảng.
 */
int util_parse_double(const char *s, double min_val, double max_val, double *out);

/*
 * Đổi chuỗi giờ 24h "HH:MM" (00:00..23:59) sang tổng số phút trong ngày (0..1439).
 * Trả về 0 nếu hợp lệ, -1 nếu sai định dạng.
 */
int util_parse_hhmm(const char *s, int *minutes_out);

/* Đổi số phút trong ngày (0..1439) thành chuỗi "HH:MM". Trả về 0 hoặc -1. */
int util_format_hhmm(int minutes, char *buf, size_t sz);

#endif /* UTIL_H */
