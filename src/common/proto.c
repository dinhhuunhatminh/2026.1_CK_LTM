#include "proto.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RAW_TOKENS (PROTO_MAX_ARGS + 3)

const char *proto_dev_type_str(dev_type_t type) {
    switch (type) {
    case DEV_SENSOR: return "SENSOR";
    case DEV_PUMP:   return "PUMP";
    case DEV_FERT:   return "FERT";
    case DEV_LIGHT:  return "LIGHT";
    default:         return "UNKNOWN";
    }
}

dev_type_t proto_dev_type_from_str(const char *s) {
    if (s == NULL) {
        return DEV_NONE;
    }
    if (strcmp(s, "SENSOR") == 0) return DEV_SENSOR;
    if (strcmp(s, "PUMP") == 0)   return DEV_PUMP;
    if (strcmp(s, "FERT") == 0)   return DEV_FERT;
    if (strcmp(s, "LIGHT") == 0)  return DEV_LIGHT;
    return DEV_NONE;
}

const char *proto_code_text(int code) {
    switch (code) {
    case PROTO_OK:            return "OK";
    case PROTO_MULTI:         return "MULTI";
    case PROTO_BAD_PASSWORD:  return "BAD PASSWORD";
    case PROTO_BAD_TOKEN:     return "BAD TOKEN";
    case PROTO_BAD_ARG:       return "BAD ARGUMENT";
    case PROTO_UNSUPPORTED:   return "UNSUPPORTED";
    case PROTO_DEVICE_OFF:    return "DEVICE OFF";
    case PROTO_TOO_MANY_APPS: return "TOO MANY APPS";
    case PROTO_LOCKED:        return "LOCKED";
    case PROTO_UNKNOWN_CMD:   return "UNKNOWN COMMAND";
    case PROTO_UNKNOWN_ERR:
    default:                  return "UNKNOWN ERROR";
    }
}

const char *proto_code_vi(int code) {
    switch (code) {
    case PROTO_OK:            return "Thành công";
    case PROTO_MULTI:         return "Thành công (nhiều dòng dữ liệu)";
    case PROTO_BAD_PASSWORD:  return "Sai mật khẩu";
    case PROTO_BAD_TOKEN:     return "Token không hợp lệ hoặc bị thiếu";
    case PROTO_BAD_ARG:       return "Tham số không hợp lệ";
    case PROTO_UNSUPPORTED:   return "Lệnh không hỗ trợ cho loại thiết bị này";
    case PROTO_DEVICE_OFF:    return "Thiết bị đang tắt";
    case PROTO_TOO_MANY_APPS: return "Vượt quá số ứng dụng xác thực tối đa";
    case PROTO_LOCKED:        return "Tạm khóa do nhập sai mật khẩu nhiều lần";
    case PROTO_UNKNOWN_CMD:   return "Lệnh không tồn tại";
    case PROTO_UNKNOWN_ERR:
    default:                  return "Lỗi không xác định";
    }
}

const char *proto_cmd_name(proto_cmd_type_t type) {
    switch (type) {
    case CMD_CONNECT:   return "CONNECT";
    case CMD_INFO:      return "INFO";
    case CMD_STATUS:    return "STATUS";
    case CMD_PARAMS:    return "PARAMS";
    case CMD_POWER:     return "POWER";
    case CMD_PASSWD:    return "PASSWD";
    case CMD_LOG:       return "LOG";
    case CMD_SET:       return "SET";
    case CMD_DATA:      return "DATA";
    case CMD_HISTORY:   return "HISTORY";
    case CMD_FEED:      return "FEED";
    case CMD_PUMP:      return "PUMP";
    case CMD_FERTILIZE: return "FERTILIZE";
    case CMD_LIGHT:     return "LIGHT";
    case CMD_NONE:
    default:            return "NONE";
    }
}

static proto_cmd_type_t cmd_type_from_str(const char *s) {
    if (strcmp(s, "CONNECT") == 0)   return CMD_CONNECT;
    if (strcmp(s, "INFO") == 0)      return CMD_INFO;
    if (strcmp(s, "STATUS") == 0)    return CMD_STATUS;
    if (strcmp(s, "PARAMS") == 0)    return CMD_PARAMS;
    if (strcmp(s, "POWER") == 0)     return CMD_POWER;
    if (strcmp(s, "PASSWD") == 0)    return CMD_PASSWD;
    if (strcmp(s, "LOG") == 0)       return CMD_LOG;
    if (strcmp(s, "SET") == 0)       return CMD_SET;
    if (strcmp(s, "DATA") == 0)      return CMD_DATA;
    if (strcmp(s, "HISTORY") == 0)   return CMD_HISTORY;
    if (strcmp(s, "FEED") == 0)      return CMD_FEED;
    if (strcmp(s, "PUMP") == 0)      return CMD_PUMP;
    if (strcmp(s, "FERTILIZE") == 0) return CMD_FERTILIZE;
    if (strcmp(s, "LIGHT") == 0)     return CMD_LIGHT;
    return CMD_NONE;
}

/* Kiểm tra chuỗi là số nguyên dương hợp lệ (trong khoảng 1..max_val). */
static int parse_positive_int(const char *s, int max_val, int *val_out) {
    char *end = NULL;
    long v;

    if (s == NULL || *s == '\0' || !isdigit((unsigned char)*s)) {
        return -1;
    }
    v = strtol(s, &end, 10);
    if (*end != '\0' || v <= 0 || v > max_val) {
        return -1;
    }
    if (val_out != NULL) {
        *val_out = (int)v;
    }
    return 0;
}

/* Kiểm tra tên khóa của lệnh SET (ngoài SENSOR được xử lý riêng). */
static int is_single_val_set_key(const char *key) {
    static const char *const keys[] = {
        "INTERVAL", "HMAX", "HMIN", "SCHEDULE", "VOLUME",
        "NMIN", "PMIN", "KMIN", "CONC", "NPK_RATIO", "WATT"
    };
    size_t i;

    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (strcmp(key, keys[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * Tách một dòng thành mảng các trường cách nhau bởi dấu cách/tab.
 * Bỏ '\r', '\n' ở cuối dòng. Trả về số trường (>= 0), hoặc -1 nếu dòng/trường quá dài.
 */
static int split_tokens(const char *line, char toks[MAX_RAW_TOKENS][PROTO_MAX_ARG]) {
    size_t len = strlen(line);
    size_t i = 0;
    int count = 0;

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        len--;
    }
    if (len > PROTO_MAX_LINE) {
        return -1;
    }

    while (i < len) {
        size_t start;
        size_t tlen;

        while (i < len && (line[i] == ' ' || line[i] == '\t')) {
            i++;
        }
        if (i >= len) {
            break;
        }
        if (count >= MAX_RAW_TOKENS) {
            return -1;
        }
        start = i;
        while (i < len && line[i] != ' ' && line[i] != '\t') {
            i++;
        }
        tlen = i - start;
        if (tlen >= PROTO_MAX_ARG) {
            return -1;
        }
        memcpy(toks[count], line + start, tlen);
        toks[count][tlen] = '\0';
        count++;
    }
    return count;
}

static int validate_cmd_args(const proto_cmd_t *cmd) {
    switch (cmd->type) {
    case CMD_INFO:
    case CMD_STATUS:
    case CMD_PARAMS:
    case CMD_DATA:
    case CMD_FERTILIZE:
        return (cmd->argc == 0) ? PROTO_OK : PROTO_BAD_ARG;

    case CMD_LOG:
    case CMD_HISTORY:
        if (cmd->argc == 0) {
            return PROTO_OK;
        }
        if (cmd->argc == 1 && parse_positive_int(cmd->argv[0], 10000, NULL) == 0) {
            return PROTO_OK;
        }
        return PROTO_BAD_ARG;

    case CMD_POWER:
    case CMD_LIGHT:
        if (cmd->argc == 1 &&
            (strcmp(cmd->argv[0], "ON") == 0 || strcmp(cmd->argv[0], "OFF") == 0)) {
            return PROTO_OK;
        }
        return PROTO_BAD_ARG;

    case CMD_PASSWD:
        return (cmd->argc == 2) ? PROTO_OK : PROTO_BAD_ARG;

    case CMD_PUMP:
        return (cmd->argc == 1) ? PROTO_OK : PROTO_BAD_ARG;

    case CMD_FEED:
        if (cmd->argc == 2 && strcmp(cmd->argv[0], "WATER") == 0) {
            return PROTO_OK;
        }
        if (cmd->argc == 4 && strcmp(cmd->argv[0], "NPK") == 0) {
            return PROTO_OK;
        }
        return PROTO_BAD_ARG;

    case CMD_SET:
        if (cmd->argc < 2) {
            return PROTO_BAD_ARG;
        }
        if (strcmp(cmd->argv[0], "SENSOR") == 0) {
            return (cmd->argc == 4) ? PROTO_OK : PROTO_BAD_ARG;
        }
        if (is_single_val_set_key(cmd->argv[0])) {
            return (cmd->argc == 2) ? PROTO_OK : PROTO_BAD_ARG;
        }
        return PROTO_BAD_ARG;

    default:
        return PROTO_BAD_ARG;
    }
}

int proto_parse_cmd(const char *line, proto_cmd_t *out) {
    char toks[MAX_RAW_TOKENS][PROTO_MAX_ARG];
    int ntoks;
    int i;
    proto_cmd_type_t ctype;

    if (line == NULL || out == NULL) {
        return PROTO_BAD_ARG;
    }
    memset(out, 0, sizeof *out);

    ntoks = split_tokens(line, toks);
    if (ntoks < 0) {
        return PROTO_BAD_ARG;
    }
    if (ntoks == 0) {
        return PROTO_UNKNOWN_CMD;
    }

    if (strcmp(toks[0], "CONNECT") == 0) {
        if (ntoks != 3) {
            return PROTO_BAD_ARG;
        }
        out->type = CMD_CONNECT;
        out->argc = 2;
        strcpy(out->argv[0], toks[1]);
        strcpy(out->argv[1], toks[2]);
        return PROTO_OK;
    }

    if (ntoks == 1) {
        if (cmd_type_from_str(toks[0]) != CMD_NONE) {
            return PROTO_BAD_TOKEN;
        }
        return PROTO_UNKNOWN_CMD;
    }

    ctype = cmd_type_from_str(toks[1]);
    if (ctype == CMD_NONE || ctype == CMD_CONNECT) {
        if (cmd_type_from_str(toks[0]) != CMD_NONE) {
            return PROTO_BAD_TOKEN;
        }
        return PROTO_UNKNOWN_CMD;
    }

    if (strlen(toks[0]) >= PROTO_MAX_TOKEN) {
        return PROTO_BAD_TOKEN;
    }
    if (ntoks - 2 > PROTO_MAX_ARGS) {
        return PROTO_BAD_ARG;
    }

    out->type = ctype;
    strcpy(out->token, toks[0]);
    out->argc = ntoks - 2;
    for (i = 0; i < out->argc; i++) {
        strcpy(out->argv[i], toks[i + 2]);
    }

    return validate_cmd_args(out);
}

static int check_set_key_for_device(const char *key, dev_type_t dev_type) {
    switch (dev_type) {
    case DEV_SENSOR:
        return strcmp(key, "INTERVAL") == 0;
    case DEV_PUMP:
        return strcmp(key, "HMAX") == 0 ||
               strcmp(key, "HMIN") == 0 ||
               strcmp(key, "SCHEDULE") == 0 ||
               strcmp(key, "VOLUME") == 0 ||
               strcmp(key, "SENSOR") == 0;
    case DEV_FERT:
        return strcmp(key, "NMIN") == 0 ||
               strcmp(key, "PMIN") == 0 ||
               strcmp(key, "KMIN") == 0 ||
               strcmp(key, "CONC") == 0 ||
               strcmp(key, "VOLUME") == 0 ||
               strcmp(key, "NPK_RATIO") == 0 ||
               strcmp(key, "SENSOR") == 0;
    case DEV_LIGHT:
        return strcmp(key, "WATT") == 0 ||
               strcmp(key, "SCHEDULE") == 0;
    default:
        return 0;
    }
}

int proto_check_device(const proto_cmd_t *cmd, dev_type_t dev_type) {
    if (cmd == NULL || dev_type == DEV_NONE) {
        return PROTO_UNSUPPORTED;
    }

    switch (cmd->type) {
    case CMD_CONNECT:
    case CMD_INFO:
    case CMD_STATUS:
    case CMD_PARAMS:
    case CMD_POWER:
    case CMD_PASSWD:
    case CMD_LOG:
        return PROTO_OK;

    case CMD_DATA:
    case CMD_HISTORY:
    case CMD_FEED:
        return (dev_type == DEV_SENSOR) ? PROTO_OK : PROTO_UNSUPPORTED;

    case CMD_PUMP:
        return (dev_type == DEV_PUMP) ? PROTO_OK : PROTO_UNSUPPORTED;

    case CMD_FERTILIZE:
        return (dev_type == DEV_FERT) ? PROTO_OK : PROTO_UNSUPPORTED;

    case CMD_LIGHT:
        return (dev_type == DEV_LIGHT) ? PROTO_OK : PROTO_UNSUPPORTED;

    case CMD_SET:
        if (cmd->argc >= 1 && check_set_key_for_device(cmd->argv[0], dev_type)) {
            return PROTO_OK;
        }
        return PROTO_UNSUPPORTED;

    default:
        return PROTO_UNSUPPORTED;
    }
}

int proto_format_reply(char *buf, size_t sz, int code, const char *detail) {
    const char *text;
    int n;

    if (buf == NULL || sz == 0) {
        return -1;
    }
    text = proto_code_text(code);
    if (detail == NULL || detail[0] == '\0') {
        n = snprintf(buf, sz, "%d %s\n", code, text);
    } else {
        n = snprintf(buf, sz, "%d %s %s\n", code, text, detail);
    }
    if (n < 0 || (size_t)n >= sz) {
        return -1;
    }
    return n;
}

int proto_format_replyf(char *buf, size_t sz, int code, const char *fmt, ...) {
    char detail[PROTO_MAX_LINE + 1];
    va_list ap;
    int n;

    if (fmt == NULL || fmt[0] == '\0') {
        return proto_format_reply(buf, sz, code, NULL);
    }
    va_start(ap, fmt);
    n = vsnprintf(detail, sizeof detail, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= sizeof detail) {
        return -1;
    }
    return proto_format_reply(buf, sz, code, detail);
}

int proto_format_multi_header(char *buf, size_t sz, const char *tag, int count) {
    int n;

    if (buf == NULL || sz == 0 || tag == NULL || tag[0] == '\0' || count < 0) {
        return -1;
    }
    n = snprintf(buf, sz, "%d %s %d\n", PROTO_MULTI, tag, count);
    if (n < 0 || (size_t)n >= sz) {
        return -1;
    }
    return n;
}

int proto_parse_reply(const char *line, proto_reply_t *out) {
    char clean[PROTO_MAX_LINE + 1];
    size_t len;
    const char *p;
    char *end = NULL;
    long code;

    if (line == NULL || out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof *out);

    len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        len--;
    }
    if (len == 0 || len > PROTO_MAX_LINE) {
        return -1;
    }
    memcpy(clean, line, len);
    clean[len] = '\0';

    if (!isdigit((unsigned char)clean[0])) {
        return -1;
    }
    code = strtol(clean, &end, 10);
    if (code < 100 || code > 999 || *end != ' ') {
        return -1;
    }
    out->code = (int)code;
    p = end + 1;
    while (*p == ' ') {
        p++;
    }
    if (*p == '\0') {
        return -1;
    }

    if (out->code == PROTO_MULTI) {
        char toks[MAX_RAW_TOKENS][PROTO_MAX_ARG];
        int ntoks = split_tokens(p, toks);
        int cnt = 0;

        if (ntoks != 2) {
            return -1;
        }
        if (strcmp(toks[1], "0") != 0 && parse_positive_int(toks[1], 100000, &cnt) != 0) {
            return -1;
        }
        if (strlen(toks[0]) >= PROTO_MAX_TOKEN) {
            return -1;
        }
        out->is_multi = 1;
        strcpy(out->multi_tag, toks[0]);
        out->multi_count = cnt;
        return 0;
    }

    /* Các mã phản hồi 1 dòng: bỏ phần TEXT chuẩn của mã nếu khớp, giữ phần detail. */
    {
        const char *std_text = proto_code_text(out->code);
        size_t std_len = strlen(std_text);

        if (strncmp(p, std_text, std_len) == 0 &&
            (p[std_len] == '\0' || p[std_len] == ' ')) {
            p += std_len;
            while (*p == ' ') {
                p++;
            }
        }
        strcpy(out->detail, p);
    }
    return 0;
}

int proto_format_scan(char *buf, size_t sz, const char *app_id) {
    int n;

    if (buf == NULL || sz == 0 || app_id == NULL || app_id[0] == '\0') {
        return -1;
    }
    n = snprintf(buf, sz, "SCAN %s\n", app_id);
    if (n < 0 || (size_t)n >= sz) {
        return -1;
    }
    return n;
}

int proto_parse_scan(const char *line, char *app_id, size_t app_id_sz) {
    char toks[MAX_RAW_TOKENS][PROTO_MAX_ARG];
    int ntoks;

    if (line == NULL || app_id == NULL || app_id_sz == 0) {
        return -1;
    }
    ntoks = split_tokens(line, toks);
    if (ntoks != 2 || strcmp(toks[0], "SCAN") != 0) {
        return -1;
    }
    if (strlen(toks[1]) >= app_id_sz) {
        return -1;
    }
    strcpy(app_id, toks[1]);
    return 0;
}

int proto_format_device(char *buf, size_t sz, const char *device_id,
                        dev_type_t type, int tcp_port) {
    const char *tstr;
    int n;

    if (buf == NULL || sz == 0 || device_id == NULL || device_id[0] == '\0' ||
        tcp_port <= 0 || tcp_port > 65535) {
        return -1;
    }
    tstr = proto_dev_type_str(type);
    if (strcmp(tstr, "UNKNOWN") == 0) {
        return -1;
    }
    n = snprintf(buf, sz, "DEVICE %s %s %d\n", device_id, tstr, tcp_port);
    if (n < 0 || (size_t)n >= sz) {
        return -1;
    }
    return n;
}

int proto_parse_device(const char *line, char *device_id, size_t id_sz,
                       dev_type_t *type, int *tcp_port) {
    char toks[MAX_RAW_TOKENS][PROTO_MAX_ARG];
    int ntoks;
    dev_type_t dt;
    int port = 0;

    if (line == NULL || device_id == NULL || id_sz == 0 ||
        type == NULL || tcp_port == NULL) {
        return -1;
    }
    ntoks = split_tokens(line, toks);
    if (ntoks != 4 || strcmp(toks[0], "DEVICE") != 0) {
        return -1;
    }
    if (strlen(toks[1]) >= id_sz) {
        return -1;
    }
    dt = proto_dev_type_from_str(toks[2]);
    if (dt == DEV_NONE) {
        return -1;
    }
    if (parse_positive_int(toks[3], 65535, &port) != 0) {
        return -1;
    }
    strcpy(device_id, toks[1]);
    *type = dt;
    *tcp_port = port;
    return 0;
}
