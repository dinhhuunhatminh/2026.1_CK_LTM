/*
 * proto.h - phân tích lệnh, bảng mã phản hồi và dựng thông điệp giao thức.
 *
 * Giao thức văn bản ASCII theo dòng (DESIGN.md mục 3.1, 3.3–3.6):
 *   - Quét UDP (:9999):
 *       controller -> broadcast : SCAN <app_id>\n
 *       device     -> unicast   : DEVICE <device_id> <type> <tcp_port>\n
 *   - Kết nối & điều khiển TCP:
 *       chưa xác thực : CONNECT <app_id> <password>\n
 *       đã xác thực   : <token> <LỆNH> [tham số...]\n
 *   - Phản hồi từ thiết bị:
 *       1 dòng        : <mã> <TEXT> [dữ liệu...]\n
 *       nhiều dòng    : 150 <NHÃN> <N>\n  theo sau bởi N dòng dữ liệu
 */
#ifndef PROTO_H
#define PROTO_H

#include <stddef.h>

#define PROTO_MAX_LINE   512
#define PROTO_MAX_TOKEN  64
#define PROTO_MAX_ARGS   6
#define PROTO_MAX_ARG    128

/* Mã phản hồi của giao thức (DESIGN.md mục 3.5). */
typedef enum {
    PROTO_OK            = 100, /* OK */
    PROTO_MULTI         = 150, /* OK, theo sau là N dòng dữ liệu (LOG, HISTORY) */
    PROTO_BAD_PASSWORD  = 200, /* BAD PASSWORD */
    PROTO_BAD_TOKEN     = 201, /* BAD TOKEN */
    PROTO_BAD_ARG       = 202, /* BAD ARGUMENT */
    PROTO_UNSUPPORTED   = 203, /* UNSUPPORTED */
    PROTO_DEVICE_OFF    = 204, /* DEVICE OFF */
    PROTO_TOO_MANY_APPS = 205, /* TOO MANY APPS */
    PROTO_LOCKED        = 206, /* LOCKED */
    PROTO_UNKNOWN_CMD   = 300, /* UNKNOWN COMMAND */
    PROTO_UNKNOWN_ERR   = 999  /* UNKNOWN ERROR */
} proto_code_t;

/* Loại thiết bị trong hệ thống (DESIGN.md mục 3.3). */
typedef enum {
    DEV_NONE = 0,
    DEV_SENSOR,
    DEV_PUMP,
    DEV_FERT,
    DEV_LIGHT
} dev_type_t;

/* Các loại lệnh TCP (DESIGN.md mục 3.4, 3.6). */
typedef enum {
    CMD_NONE = 0,
    CMD_CONNECT,   /* CONNECT <app_id> <password> */
    CMD_INFO,      /* <token> INFO */
    CMD_STATUS,    /* <token> STATUS */
    CMD_PARAMS,    /* <token> PARAMS */
    CMD_POWER,     /* <token> POWER ON|OFF */
    CMD_PASSWD,    /* <token> PASSWD <cũ> <mới> */
    CMD_LOG,       /* <token> LOG [n] */
    CMD_SET,       /* <token> SET <khóa> <giá_trị...> */
    CMD_DATA,      /* <token> DATA */
    CMD_HISTORY,   /* <token> HISTORY [n] */
    CMD_FEED,      /* <token> FEED WATER <lít> | FEED NPK <N> <P> <K> */
    CMD_PUMP,      /* <token> PUMP <lít> */
    CMD_FERTILIZE, /* <token> FERTILIZE */
    CMD_LIGHT      /* <token> LIGHT ON|OFF */
} proto_cmd_type_t;

/* Kết quả phân tích một dòng lệnh TCP. */
typedef struct {
    proto_cmd_type_t type;
    char             token[PROTO_MAX_TOKEN]; /* rỗng với CMD_CONNECT */
    int              argc;
    char             argv[PROTO_MAX_ARGS][PROTO_MAX_ARG];
} proto_cmd_t;

/* Kết quả phân tích dòng phản hồi đầu tiên từ thiết bị. */
typedef struct {
    int  code;                        /* mã số: 100, 150, 200... */
    int  is_multi;                    /* 1 nếu code == 150 */
    char multi_tag[PROTO_MAX_TOKEN];  /* nhãn sau 150, vd: "LOG", "HISTORY" */
    int  multi_count;                 /* số dòng dữ liệu N đi kèm phản hồi 150 */
    char detail[PROTO_MAX_LINE + 1];  /* phần dữ liệu sau "<mã> <TEXT>" (nếu có) */
} proto_reply_t;

/* Chuyển đổi giữa loại thiết bị và chuỗi ("SENSOR", "PUMP", "FERT", "LIGHT"). */
const char *proto_dev_type_str(dev_type_t type);
dev_type_t  proto_dev_type_from_str(const char *s);

/* Trả về chuỗi tiếng Anh chuẩn của mã phản hồi (vd: 200 -> "BAD PASSWORD"). */
const char *proto_code_text(int code);

/* Trả về mô tả tiếng Việt thân thiện cho người dùng trên controller. */
const char *proto_code_vi(int code);

/* Trả về tên lệnh dạng chuỗi (vd: CMD_CONNECT -> "CONNECT"). */
const char *proto_cmd_name(proto_cmd_type_t type);

/*
 * Phân tích một dòng lệnh TCP (không gồm '\n'; nếu còn "\r\n" ở cuối sẽ tự cắt).
 * Trả về:
 *   - PROTO_OK (100)          nếu cú pháp hợp lệ (kết quả ghi vào *out)
 *   - PROTO_BAD_TOKEN (201)   nếu thiếu hoặc quá dài token
 *   - PROTO_BAD_ARG (202)     nếu thiếu/thừa tham số hoặc tham số sai khuôn dạng
 *   - PROTO_UNKNOWN_CMD (300) nếu tên lệnh không tồn tại
 */
int proto_parse_cmd(const char *line, proto_cmd_t *out);

/*
 * Kiểm tra lệnh đã parse có được hỗ trợ bởi loại thiết bị dev_type hay không.
 * Trả về PROTO_OK (100) nếu hỗ trợ, hoặc PROTO_UNSUPPORTED (203) nếu không áp dụng.
 */
int proto_check_device(const proto_cmd_t *cmd, dev_type_t dev_type);

/*
 * Dựng một dòng phản hồi kết thúc bằng '\n' vào buf (kích thước sz):
 *   "<mã> <TEXT>\n"            nếu detail == NULL hoặc rỗng
 *   "<mã> <TEXT> <detail>\n"   nếu có detail
 * Trả về độ dài chuỗi (không tính '\0'), hoặc -1 nếu tham số sai / bộ đệm không đủ.
 */
int proto_format_reply(char *buf, size_t sz, int code, const char *detail);

/* Dựng dòng phản hồi có định dạng kiểu printf cho phần dữ liệu đi kèm. */
int proto_format_replyf(char *buf, size_t sz, int code, const char *fmt, ...);

/*
 * Dựng dòng đầu của phản hồi nhiều dòng: "150 <tag> <count>\n".
 * Trả về độ dài chuỗi (không tính '\0'), hoặc -1 nếu lỗi.
 */
int proto_format_multi_header(char *buf, size_t sz, const char *tag, int count);

/*
 * Phân tích dòng phản hồi từ thiết bị (dùng ở phía controller).
 * Trả về 0 nếu hợp lệ, -1 nếu sai định dạng.
 */
int proto_parse_reply(const char *line, proto_reply_t *out);

/*
 * Quét UDP (DESIGN.md mục 3.3):
 *   - proto_format_scan   : dựng "SCAN <app_id>\n"
 *   - proto_parse_scan    : đọc "SCAN <app_id>" -> lấy app_id
 *   - proto_format_device : dựng "DEVICE <device_id> <type> <tcp_port>\n"
 *   - proto_parse_device  : đọc "DEVICE <device_id> <type> <tcp_port>"
 * Các hàm format trả về độ dài hoặc -1; các hàm parse trả về 0 hoặc -1.
 */
int proto_format_scan(char *buf, size_t sz, const char *app_id);
int proto_parse_scan(const char *line, char *app_id, size_t app_id_sz);
int proto_format_device(char *buf, size_t sz, const char *device_id,
                        dev_type_t type, int tcp_port);
int proto_parse_device(const char *line, char *device_id, size_t id_sz,
                       dev_type_t *type, int *tcp_port);

#endif /* PROTO_H */
