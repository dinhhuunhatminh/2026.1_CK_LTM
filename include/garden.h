/*
 * garden.h - quản lý thông tin các vườn cây trồng và danh sách thiết bị trong vườn.
 *
 * Theo DESIGN.md mục 5.2:
 *   - Mỗi vườn gồm: id, name (tên vườn), crop (loại cây), area (diện tích m²),
 *     note (ghi chú), và danh sách tối đa GARDEN_MAX_DEVICES (32) thiết bị.
 *   - Mỗi thiết bị trong vườn gồm: device_id, type, host, port, token (có thể rỗng).
 *     Không lưu mật khẩu thiết bị.
 *   - Lưu bền dạng văn bản theo từng block trong <dir>/gardens.txt
 *     (mặc định: data/controller/gardens.txt).
 */
#ifndef GARDEN_H
#define GARDEN_H

#include <stddef.h>

#include "proto.h"

#define GARDEN_MAX_COUNT    32
#define GARDEN_MAX_DEVICES  32
#define GARDEN_ID_MAX       16
#define GARDEN_NAME_MAX     64
#define GARDEN_CROP_MAX     64
#define GARDEN_NOTE_MAX     128
#define GARDEN_HOST_MAX     64
#define GARDEN_TOKEN_MAX    32
#define GARDEN_DEFAULT_DIR  "data/controller"
#define GARDEN_FILENAME     "gardens.txt"

/* Mã trả về của garden_load. */
#define GARDEN_OK         0
#define GARDEN_NOT_FOUND  1
#define GARDEN_ERR       (-1)

typedef struct {
    char       device_id[GARDEN_ID_MAX + 1];
    dev_type_t type;
    char       host[GARDEN_HOST_MAX + 1];
    int        port;
    char       token[GARDEN_TOKEN_MAX + 1]; /* rỗng nếu chưa xác thực */
} garden_device_t;

typedef struct {
    int             id;
    char            name[GARDEN_NAME_MAX + 1];
    char            crop[GARDEN_CROP_MAX + 1];
    double          area;
    char            note[GARDEN_NOTE_MAX + 1];
    int             device_count;
    garden_device_t devices[GARDEN_MAX_DEVICES];
} garden_t;

typedef struct {
    int      count;
    int      next_id;
    garden_t items[GARDEN_MAX_COUNT];
} garden_list_t;

/* Khởi tạo danh sách vườn rỗng. */
void garden_list_init(garden_list_t *list);

/*
 * Thêm một vườn mới.
 *   name, crop : chuỗi không rỗng, không chứa ký tự xuống dòng.
 *   area       : diện tích (m²), phải > 0.
 *   note       : ghi chú (có thể NULL hoặc rỗng).
 *   id_out     : nhận ID của vườn vừa tạo (có thể NULL).
 * Trả về 0 nếu thành công, -1 nếu tham số không hợp lệ hoặc đã đạt GARDEN_MAX_COUNT.
 */
int garden_add(garden_list_t *list, const char *name, const char *crop,
               double area, const char *note, int *id_out);

/*
 * Cập nhật thông tin vườn có ID là garden_id (giữ nguyên danh sách thiết bị).
 * Trả về 0 nếu thành công, -1 nếu không tìm thấy hoặc tham số không hợp lệ.
 */
int garden_update(garden_list_t *list, int garden_id, const char *name,
                  const char *crop, double area, const char *note);

/* Xóa vườn có ID là garden_id. Trả về 0 nếu thành công, -1 nếu không tìm thấy. */
int garden_remove(garden_list_t *list, int garden_id);

/* Tìm vườn theo ID. Trả về con trỏ tới vườn, hoặc NULL nếu không có. */
garden_t       *garden_find(garden_list_t *list, int garden_id);
const garden_t *garden_find_const(const garden_list_t *list, int garden_id);

/*
 * Thêm thiết bị vào vườn g (không cho phép trùng device_id trong cùng một vườn,
 * tối đa GARDEN_MAX_DEVICES thiết bị).
 * token có thể là NULL hoặc "" nếu chưa kết nối.
 * Trả về 0 nếu thành công, -1 nếu lỗi.
 */
int garden_add_device(garden_t *g, const char *device_id, dev_type_t type,
                      const char *host, int port, const char *token);

/* Xóa thiết bị có device_id khỏi vườn g. Trả về 0 nếu thành công, -1 nếu không có. */
int garden_remove_device(garden_t *g, const char *device_id);

/* Tìm thiết bị theo device_id trong vườn g. Trả về NULL nếu không thấy. */
garden_device_t *garden_find_device(garden_t *g, const char *device_id);

/* Cập nhật hoặc xóa (khi token == NULL hoặc "") token của thiết bị trong vườn. */
int garden_set_device_token(garden_t *g, const char *device_id, const char *token);

/* Định dạng danh sách tóm tắt các vườn vào buf (dùng cho hiển thị CLI). */
int garden_format_list(const garden_list_t *list, char *buf, size_t sz);

/* Định dạng thông tin chi tiết của một vườn và các thiết bị vào buf. */
int garden_format_detail(const garden_t *g, char *buf, size_t sz);

/*
 * Lưu toàn bộ danh sách vườn ra <dir>/gardens.txt (tạo thư mục nếu chưa có).
 * Trả về 0 nếu thành công, -1 nếu lỗi.
 */
int garden_save(const garden_list_t *list, const char *dir);

/*
 * Đọc danh sách vườn từ <dir>/gardens.txt.
 * Trả về:
 *   GARDEN_OK (0)        : đọc thành công vào *list
 *   GARDEN_NOT_FOUND (1) : chưa có file (*list được khởi tạo rỗng)
 *   GARDEN_ERR (-1)      : tham số sai hoặc file hỏng (*list giữ nguyên)
 */
int garden_load(garden_list_t *list, const char *dir);

#endif /* GARDEN_H */
