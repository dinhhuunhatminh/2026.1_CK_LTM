/*
 * linebuf.h - bộ đệm dòng cho luồng TCP.
 *
 * TCP là dòng byte, không giữ ranh giới thông điệp: một recv() có thể trả về
 * nửa dòng, hoặc nhiều dòng dính nhau. linebuf ghép các mảnh dữ liệu lại và
 * gọi một hàm callback cho mỗi dòng hoàn chỉnh (kết thúc bằng '\n').
 *
 * Quy ước (DESIGN.md mục 3.1, 3.2):
 *   - Dòng kết thúc bằng '\n'; '\r' ngay trước '\n' được bỏ đi (chấp nhận "\r\n").
 *   - Nội dung dòng (không tính "\r\n") dài tối đa LINEBUF_MAX_LINE byte.
 *   - Dòng dài hơn: phần thừa bị bỏ đến hết dòng, callback được gọi MỘT lần
 *     với trạng thái LINEBUF_TOO_LONG.
 *   - Dòng chứa byte '\0': callback được gọi với trạng thái LINEBUF_BAD_BYTE.
 *
 * Cách dùng:
 *
 *     linebuf_t lb;
 *     linebuf_init(&lb);
 *     ...
 *     n = recv(fd, buf, sizeof buf, 0);
 *     linebuf_feed(&lb, buf, (size_t)n, on_line, ctx);
 */
#ifndef LINEBUF_H
#define LINEBUF_H

#include <stddef.h>

#define LINEBUF_MAX_LINE 512

/* Trạng thái truyền cho callback. */
#define LINEBUF_OK       0 /* dòng hợp lệ */
#define LINEBUF_TOO_LONG 1 /* dòng quá dài, nội dung đã bị bỏ (line = "") */
#define LINEBUF_BAD_BYTE 2 /* dòng chứa '\0', nội dung đã bị bỏ (line = "") */

/*
 * Hàm xử lý mỗi dòng.
 *   line   : chuỗi kết thúc bằng '\0', không có "\n" hay "\r\n". Chỉ hợp lệ trong lúc gọi.
 *   len    : độ dài của line.
 *   status : LINEBUF_OK, LINEBUF_TOO_LONG hoặc LINEBUF_BAD_BYTE.
 *   ctx    : con trỏ người dùng truyền vào linebuf_feed.
 */
typedef void (*linebuf_cb)(const char *line, size_t len, int status, void *ctx);

typedef struct {
    char   data[LINEBUF_MAX_LINE + 2]; /* nội dung + có thể 1 byte '\r' + chừa chỗ cho '\0' */
    size_t len;                        /* số byte đang giữ của dòng dở dang */
    int    too_long;                   /* đang bỏ phần thừa của dòng quá dài */
    int    bad_byte;                   /* dòng hiện tại có byte '\0' */
} linebuf_t;

/* Đưa bộ đệm về trạng thái rỗng. */
void linebuf_init(linebuf_t *lb);

/*
 * Nạp n byte dữ liệu vừa nhận. Với mỗi dòng hoàn chỉnh tìm thấy, gọi cb theo đúng
 * thứ tự xuất hiện. Phần dở dang cuối cùng được giữ lại cho lần nạp sau.
 * Nếu lb hoặc cb là NULL, hoặc data là NULL, hàm không làm gì.
 */
void linebuf_feed(linebuf_t *lb, const char *data, size_t n, linebuf_cb cb, void *ctx);

/* Số byte của dòng dở dang đang chờ '\n' (0 nếu chưa có gì). */
size_t linebuf_pending(const linebuf_t *lb);

#endif /* LINEBUF_H */
