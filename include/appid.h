/*
 * appid.h - sinh và lưu bền ID ngẫu nhiên của ứng dụng điều khiển (controller).
 *
 * Theo DESIGN.md mục 3.1 và 5.2:
 *   - Mỗi bản cài đặt controller có một app_id gồm 16 ký tự hex ngẫu nhiên.
 *   - Sinh ở lần chạy đầu tiên và lưu vào <dir>/app_id.txt (mặc định:
 *     data/controller/app_id.txt) để dùng lại ở các lần chạy sau.
 *   - Nếu file chưa tồn tại, rỗng hoặc chứa nội dung sai định dạng (không phải
 *     16 ký tự hex), module tự động sinh ID mới và ghi đè lại file.
 */
#ifndef APPID_H
#define APPID_H

#include <stddef.h>

#define APPID_LEN         16
#define APPID_BUF_SIZE    (APPID_LEN + 1)
#define APPID_DEFAULT_DIR "data/controller"
#define APPID_FILENAME    "app_id.txt"

/*
 * Đọc hoặc sinh mới ID ứng dụng trong thư mục dir.
 *   dir    : đường dẫn thư mục lưu file app_id.txt (không được NULL hoặc rỗng).
 *   out_id : bộ đệm nhận chuỗi ID (cần kích thước >= APPID_BUF_SIZE).
 *   out_sz : kích thước bộ đệm out_id.
 * Trả về 0 nếu thành công, -1 nếu tham số không hợp lệ hoặc không ghi được file.
 */
int appid_get(const char *dir, char *out_id, size_t out_sz);

#endif /* APPID_H */
