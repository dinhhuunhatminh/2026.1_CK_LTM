/*
 * session.h - bảng ứng dụng đã xác thực của một thiết bị: app_id <-> token,
 *             giới hạn số ứng dụng và khóa chống dò mật khẩu.
 *
 * Quy tắc (DESIGN.md mục 3.4):
 *   - Mật khẩu đúng: thiết bị cấp token ngẫu nhiên 16 ký tự hex gắn với app_id.
 *     Nếu app_id đã có token thì cấp lại token cũ (kết nối lại không mất phiên).
 *   - Tối đa SESSION_MAX_APPS ứng dụng xác thực đồng thời; vượt quá -> 205.
 *   - Sai mật khẩu SESSION_MAX_FAILS lần liên tiếp từ cùng app_id -> khóa
 *     SESSION_LOCK_SEC giây (giờ giả lập), trả 206 kể cả khi sau đó nhập đúng.
 *   - Đổi mật khẩu thành công: thu hồi mọi token khác (session_revoke_others).
 *
 * Module này không đọc đồng hồ và không so mật khẩu: server truyền vào thời gian
 * hiện tại (giây) và kết quả so khớp mật khẩu (password_ok), nên test được bằng
 * giá trị cho trước, không cần mạng.
 */
#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>

#define SESSION_MAX_APPS   16 /* số ứng dụng xác thực đồng thời */
#define SESSION_MAX_TRACK  32 /* số app_id được theo dõi đếm lần sai mật khẩu */
#define SESSION_MAX_FAILS   5 /* số lần sai liên tiếp thì bị khóa */
#define SESSION_LOCK_SEC   30 /* thời gian khóa (giây) */
#define SESSION_ID_MAX     16 /* độ dài tối đa app_id */
#define SESSION_TOKEN_LEN  16 /* độ dài token (ký tự hex) */

typedef struct {
    int  used;
    char app_id[SESSION_ID_MAX + 1];
    char token[SESSION_TOKEN_LEN + 1];
} session_app_t;

/* Bộ đếm lần sai mật khẩu của một app_id. */
typedef struct {
    int     used;
    char    app_id[SESSION_ID_MAX + 1];
    int     fails;        /* số lần sai liên tiếp hiện tại */
    int64_t locked_until; /* 0 nếu không bị khóa, ngược lại là mốc mở khóa (giây) */
    int64_t last_sec;     /* lần sai gần nhất, để thay thế bản ghi cũ nhất khi đầy */
} session_fail_t;

typedef struct {
    session_app_t  apps[SESSION_MAX_APPS];
    session_fail_t fails[SESSION_MAX_TRACK];
} session_t;

/* Đưa bảng về trạng thái rỗng. */
void session_init(session_t *s);

/*
 * Xử lý một yêu cầu CONNECT.
 *   app_id      : ID ứng dụng (1..16 ký tự [A-Za-z0-9_-]).
 *   password_ok : 1 nếu mật khẩu đã được server so khớp đúng, 0 nếu sai.
 *   now_sec     : thời gian hiện tại (giây, giờ giả lập).
 *   token_out   : bộ đệm >= SESSION_TOKEN_LEN + 1 byte, nhận token khi thành công.
 * Trả về mã phản hồi của giao thức:
 *   PROTO_OK (100)            thành công, token_out có giá trị
 *   PROTO_BAD_PASSWORD (200)  sai mật khẩu (cộng dồn số lần sai)
 *   PROTO_BAD_ARG (202)       tham số NULL hoặc app_id sai định dạng
 *   PROTO_TOO_MANY_APPS (205) đã đủ SESSION_MAX_APPS ứng dụng và app_id này chưa có
 *   PROTO_LOCKED (206)        app_id đang bị khóa
 *   PROTO_UNKNOWN_ERR (999)   không sinh được token ngẫu nhiên
 */
int session_connect(session_t *s, const char *app_id, int password_ok,
                    int64_t now_sec, char *token_out);

/*
 * Kiểm tra token. Trả về app_id gắn với token nếu hợp lệ, NULL nếu token
 * sai định dạng, không tồn tại hoặc đã bị thu hồi.
 */
const char *session_validate(const session_t *s, const char *token);

/* Có đang bị khóa tại thời điểm now_sec không (1 = đang khóa, 0 = không). */
int session_is_locked(const session_t *s, const char *app_id, int64_t now_sec);

/*
 * Thu hồi mọi token trừ token keep_token (NULL nghĩa là thu hồi tất cả).
 * Ứng dụng bị thu hồi phải CONNECT lại. Trả về số token đã thu hồi.
 */
int session_revoke_others(session_t *s, const char *keep_token);

/* Số ứng dụng đang được xác thực. */
int session_count(const session_t *s);

#endif /* SESSION_H */
