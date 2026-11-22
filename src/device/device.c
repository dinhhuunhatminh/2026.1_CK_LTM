#include "device.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "util.h"

#define PARAMS_DETAIL_MAX 480 /* chừa chỗ cho "100 OK " để cả dòng không vượt 512 byte */

static void set_err(char *err, size_t sz, const char *fmt, const char *arg) {
    if (err && sz > 0) {
        snprintf(err, sz, fmt, arg ? arg : "");
    }
}

int device_init(device_t *d, const device_config_t *cfg, int64_t real_now_ms,
                char *err, size_t err_sz) {
    char path[DEVICE_PATH_MAX + DEVICE_ID_MAX + 8];
    int loaded = 0;

    if (!d || !cfg) {
        return -1;
    }
    if (cfg->type == DEV_NONE || !util_valid_id(cfg->id, DEVICE_ID_MAX) ||
        cfg->timescale <= 0 || cfg->log_dir[0] == '\0') {
        set_err(err, err_sz, "tham số thiết bị không hợp lệ%s", "");
        return -1;
    }

    memset(d, 0, sizeof *d);
    d->type = cfg->type;
    util_str_copy(d->id, sizeof d->id, cfg->id);
    store_init(&d->store);
    session_init(&d->sess);
    if (simclock_init(&d->clock, real_now_ms, 0, cfg->timescale) != 0 ||
        util_str_copy(d->state_dir, sizeof d->state_dir, cfg->state_dir) != 0) {
        set_err(err, err_sz, "tham số thiết bị không hợp lệ%s", "");
        return -1;
    }

    if (d->state_dir[0]) {
        int rc = store_load(&d->store, d->state_dir, d->id);
        if (rc == STORE_ERR) {
            set_err(err, err_sz, "file trạng thái bị hỏng: %s", d->state_dir);
            return -2;
        }
        loaded = (rc == STORE_OK);
    }
    if (!loaded) {
        if (store_set_password(&d->store, cfg->password) != 0) {
            set_err(err, err_sz,
                    "cần --password hợp lệ (4-32 ký tự, không có dấu cách)%s", "");
            return -2;
        }
        if (d->state_dir[0] && store_save(&d->store, d->state_dir, d->id) != 0) {
            set_err(err, err_sz, "không ghi được file trạng thái trong: %s", d->state_dir);
            return -2;
        }
    }

    if (mkdir(cfg->log_dir, 0755) != 0 && errno != EEXIST) {
        set_err(err, err_sz, "không tạo được thư mục log: %s", cfg->log_dir);
        return -2;
    }
    if (snprintf(path, sizeof path, "%s/%s.log", cfg->log_dir, d->id) >= (int)sizeof path ||
        log_init(&d->log, path) != 0) {
        set_err(err, err_sz, "không mở được file log trong: %s", cfg->log_dir);
        return -2;
    }

    d->power = 1;
    d->start_sim_sec = simclock_now_sec(&d->clock, real_now_ms);
    if (log_write(&d->log, d->start_sim_sec, LOG_SRC_SYS, "started type=%s id=%s timescale=%g%s",
                  proto_dev_type_str(d->type), d->id, cfg->timescale,
                  loaded ? " (state loaded)" : "") != 0) {
        set_err(err, err_sz, "không ghi được file log trong: %s", cfg->log_dir);
        return -2; /* log_init không mở file, nên phải thử ghi để phát hiện thư mục không ghi được */
    }
    return 0;
}

void device_log(device_t *d, int64_t real_now_ms, log_source_t src, const char *msg) {
    if (d && msg) {
        log_write(&d->log, simclock_now_sec(&d->clock, real_now_ms), src, "%s", msg);
    }
}

/* Rút gọn app_id để ghi log; app_id sai định dạng không được đưa vào log. */
static void app_tag(const char *app_id, char out[9]) {
    if (app_id && util_valid_id(app_id, SESSION_ID_MAX)) {
        snprintf(out, 9, "%.8s", app_id);
    } else {
        snprintf(out, 9, "invalid");
    }
}

static int reply(char *out, size_t sz, int code, const char *detail) {
    return proto_format_reply(out, sz, code, detail);
}

static int do_connect(device_t *d, const proto_cmd_t *cmd, int64_t now,
                      char *out, size_t sz) {
    char token[SESSION_TOKEN_LEN + 1] = "";
    char tag[9];
    int ok = store_password_matches(&d->store, cmd->argv[1]);
    int rc = session_connect(&d->sess, cmd->argv[0], ok, now, token);

    app_tag(cmd->argv[0], tag);
    switch (rc) {
    case PROTO_OK:
        log_write(&d->log, now, LOG_SRC_AUTH, "app=%s connected", tag);
        return proto_format_replyf(out, sz, PROTO_OK, "%s %s %s %s", token, d->id,
                                   proto_dev_type_str(d->type), DEVICE_VERSION);
    case PROTO_BAD_PASSWORD:
        log_write(&d->log, now, LOG_SRC_AUTH, "app=%s wrong password", tag);
        break;
    case PROTO_LOCKED:
        log_write(&d->log, now, LOG_SRC_AUTH, "app=%s refused: locked after repeated failures", tag);
        break;
    case PROTO_TOO_MANY_APPS:
        log_write(&d->log, now, LOG_SRC_AUTH, "app=%s refused: too many apps", tag);
        break;
    default:
        break;
    }
    return reply(out, sz, rc, NULL);
}

static int do_info(const device_t *d, int64_t now, char *out, size_t sz) {
    return proto_format_replyf(out, sz, PROTO_OK, "id=%s type=%s version=%s uptime=%lld",
                               d->id, proto_dev_type_str(d->type), DEVICE_VERSION,
                               (long long)(now - d->start_sim_sec));
}

static int do_status(const device_t *d, char *out, size_t sz) {
    return proto_format_replyf(out, sz, PROTO_OK, "power=%s", d->power ? "ON" : "OFF");
}

static int do_params(const device_t *d, char *out, size_t sz) {
    char detail[PARAMS_DETAIL_MAX + 1];
    size_t len = 0;

    detail[0] = '\0';
    for (int i = 0; i < d->store.count; i++) {
        char item[STORE_KEY_MAX + STORE_VAL_MAX + 3];
        int n = snprintf(item, sizeof item, "%s%s=%s", len ? " " : "",
                         d->store.kv[i].key, d->store.kv[i].value);
        if (n < 0 || len + (size_t)n > PARAMS_DETAIL_MAX) {
            break; /* hết chỗ trong một dòng: bỏ phần còn lại */
        }
        memcpy(detail + len, item, (size_t)n + 1);
        len += (size_t)n;
    }
    return reply(out, sz, PROTO_OK, detail);
}

static int do_power(device_t *d, const proto_cmd_t *cmd, const char *tag, int64_t now,
                    char *out, size_t sz) {
    d->power = (strcmp(cmd->argv[0], "ON") == 0);
    log_write(&d->log, now, LOG_SRC_CMD, "app=%s POWER %s", tag, d->power ? "ON" : "OFF");
    return reply(out, sz, PROTO_OK, NULL);
}

static int do_passwd(device_t *d, const proto_cmd_t *cmd, const char *tag, int64_t now,
                     char *out, size_t sz) {
    char old_pw[STORE_PASS_MAX + 1];
    int revoked;

    if (!store_password_matches(&d->store, cmd->argv[0])) {
        log_write(&d->log, now, LOG_SRC_AUTH, "app=%s PASSWD refused: wrong old password", tag);
        return reply(out, sz, PROTO_BAD_PASSWORD, NULL);
    }
    memcpy(old_pw, d->store.password, sizeof old_pw);
    if (store_set_password(&d->store, cmd->argv[1]) != 0) {
        return reply(out, sz, PROTO_BAD_ARG, "new password must be 4-32 chars without spaces");
    }
    if (d->state_dir[0] && store_save(&d->store, d->state_dir, d->id) != 0) {
        memcpy(d->store.password, old_pw, sizeof old_pw); /* không lưu được: giữ mật khẩu cũ */
        log_write(&d->log, now, LOG_SRC_WARN, "app=%s PASSWD failed: cannot save state", tag);
        return reply(out, sz, PROTO_UNKNOWN_ERR, "cannot save state");
    }
    revoked = session_revoke_others(&d->sess, cmd->token);
    log_write(&d->log, now, LOG_SRC_AUTH, "app=%s password changed, revoked %d other session(s)",
              tag, revoked);
    return reply(out, sz, PROTO_OK, NULL);
}

static int do_log(device_t *d, const proto_cmd_t *cmd, char *out, size_t sz) {
    int want = 20;
    int got, len, n;
    char (*lines)[LOG_MAX_LINE];

    if (cmd->argc == 1 &&
        util_parse_int(cmd->argv[0], 1, DEVICE_LOG_MAX_LINES, &want) != 0) {
        want = DEVICE_LOG_MAX_LINES; /* n hợp lệ nhưng vượt mức cho phép: lấy mức tối đa */
    }
    lines = malloc((size_t)want * sizeof *lines);
    if (!lines) {
        return reply(out, sz, PROTO_UNKNOWN_ERR, NULL);
    }
    got = log_tail(&d->log, lines, want);
    if (got < 0) {
        free(lines);
        return reply(out, sz, PROTO_UNKNOWN_ERR, "cannot read log");
    }
    len = proto_format_multi_header(out, sz, "LOG", got);
    for (int i = 0; len >= 0 && i < got; i++) {
        n = snprintf(out + len, sz - (size_t)len, "%s\n", lines[i]);
        len = (n < 0 || (size_t)n >= sz - (size_t)len) ? -1 : len + n;
    }
    free(lines);
    return len;
}

int device_handle_line(device_t *d, const char *line, int64_t real_now_ms,
                       char *out, size_t out_sz) {
    proto_cmd_t cmd;
    const char *app;
    char tag[9];
    int64_t now;
    int rc;

    if (!d || !line || !out || out_sz == 0) {
        return -1;
    }
    now = simclock_now_sec(&d->clock, real_now_ms);
    rc = proto_parse_cmd(line, &cmd);
    if (rc != PROTO_OK) {
        return reply(out, out_sz, rc, NULL);
    }
    if (cmd.type == CMD_CONNECT) {
        return do_connect(d, &cmd, now, out, out_sz);
    }

    app = session_validate(&d->sess, cmd.token);
    if (!app) {
        return reply(out, out_sz, PROTO_BAD_TOKEN, NULL);
    }
    app_tag(app, tag);
    if (proto_check_device(&cmd, d->type) != PROTO_OK) {
        return reply(out, out_sz, PROTO_UNSUPPORTED, NULL);
    }

    switch (cmd.type) {
    case CMD_INFO:   return do_info(d, now, out, out_sz);
    case CMD_STATUS: return do_status(d, out, out_sz);
    case CMD_PARAMS: return do_params(d, out, out_sz);
    case CMD_POWER:  return do_power(d, &cmd, tag, now, out, out_sz);
    case CMD_PASSWD: return do_passwd(d, &cmd, tag, now, out, out_sz);
    case CMD_LOG:    return do_log(d, &cmd, out, out_sz);
    default:
        /* Lệnh riêng của loại thiết bị: sẽ làm ở các commit dev_*. */
        if (!d->power) {
            return reply(out, out_sz, PROTO_DEVICE_OFF, NULL);
        }
        return reply(out, out_sz, PROTO_UNSUPPORTED, "not implemented yet");
    }
}
