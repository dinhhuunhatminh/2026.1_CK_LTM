/*
 * store.h - lưu và đọc trạng thái bền của một thiết bị (mật khẩu, tham số).
 *
 * Theo DESIGN.md mục 4.1: nếu có file <state_dir>/<device_id>.state thì giá trị
 * trong file được ưu tiên hơn mặc định. Nhờ vậy mật khẩu đã đổi và tham số đã
 * chỉnh còn nguyên sau khi khởi động lại thiết bị.
 *
 * Định dạng file (văn bản, mỗi dòng một cặp khóa=giá trị):
 *
 *     # device state v1
 *     password=1234
 *     HMIN=30
 *     SCHEDULE=06:00,17:00
 *
 *   - Dòng trống và dòng bắt đầu bằng '#' bị bỏ qua; chấp nhận "\r\n".
 *   - Khóa: 1..STORE_KEY_MAX ký tự [A-Za-z0-9_.-]. Khóa "password" dành riêng cho mật khẩu.
 *   - Giá trị: tối đa STORE_VAL_MAX ký tự in được (cho phép dấu cách), không có xuống dòng.
 *
 * Ghi file an toàn: ghi ra <file>.tmp với quyền 0600 rồi rename, nên file đang có
 * không bao giờ bị ghi dở nếu tiến trình chết giữa chừng. Mật khẩu lưu dạng văn
 * bản (đồ án không dùng thư viện mật mã), nên file chỉ cho chủ sở hữu đọc.
 *
 * Module chỉ làm việc với file, không dùng mạng và không đọc đồng hồ.
 */
#ifndef STORE_H
#define STORE_H

#define STORE_MAX_KV    32  /* số tham số tối đa */
#define STORE_KEY_MAX   32
#define STORE_VAL_MAX   128
#define STORE_PASS_MIN  4
#define STORE_PASS_MAX  32
#define STORE_LINE_MAX  256 /* độ dài tối đa một dòng trong file */

/* Mã trả về của store_load. */
#define STORE_OK         0
#define STORE_ERR       (-1) /* lỗi hoặc file hỏng */
#define STORE_NOT_FOUND  1   /* chưa có file trạng thái (lần chạy đầu) */

typedef struct {
    char key[STORE_KEY_MAX + 1];
    char value[STORE_VAL_MAX + 1];
} store_kv_t;

typedef struct {
    char       password[STORE_PASS_MAX + 1]; /* rỗng nếu chưa đặt */
    int        count;
    store_kv_t kv[STORE_MAX_KV];
} store_t;

/* Đưa về trạng thái rỗng (chưa có mật khẩu, không có tham số). */
void store_init(store_t *s);

/*
 * Đặt mật khẩu. Hợp lệ khi dài STORE_PASS_MIN..STORE_PASS_MAX ký tự in được,
 * không chứa khoảng trắng. Trả về 0 nếu thành công, -1 nếu không hợp lệ.
 */
int store_set_password(store_t *s, const char *password);

/* Mật khẩu có khớp không (1 = khớp, 0 = sai hoặc chưa đặt mật khẩu). */
int store_password_matches(const store_t *s, const char *password);

/*
 * Đặt hoặc ghi đè tham số. Trả về 0 nếu thành công, -1 nếu khóa/giá trị không
 * hợp lệ, khóa là "password", hoặc bảng đã đầy (STORE_MAX_KV tham số).
 */
int store_set(store_t *s, const char *key, const char *value);

/* Lấy giá trị tham số, NULL nếu không có. */
const char *store_get(const store_t *s, const char *key);

/*
 * Ghi trạng thái ra <dir>/<device_id>.state (tạo thư mục dir nếu chưa có).
 * device_id phải qua util_valid_id (chặn đường dẫn kiểu "../x"). Cần có mật khẩu.
 * Trả về 0 nếu thành công, -1 nếu lỗi.
 */
int store_save(const store_t *s, const char *dir, const char *device_id);

/*
 * Đọc trạng thái từ <dir>/<device_id>.state.
 * Trả về STORE_OK (đã nạp vào *s), STORE_NOT_FOUND (không có file, *s giữ nguyên)
 * hoặc STORE_ERR (file hỏng hoặc lỗi đọc, *s giữ nguyên).
 */
int store_load(store_t *s, const char *dir, const char *device_id);

#endif /* STORE_H */
