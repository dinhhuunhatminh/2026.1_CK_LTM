#include "options.h"

#include <ctype.h>
#include <string.h>

#include "server.h"
#include "util.h"

static void fail(char *err, size_t sz, const char *fmt, const char *arg) {
    if (err && sz > 0) {
        snprintf(err, sz, fmt, arg);
    }
}

/* Đổi chuỗi sang chữ hoa để --type nhận cả "pump" lẫn "PUMP". */
static int to_upper(char *dst, size_t sz, const char *src) {
    size_t n = strlen(src);
    if (n >= sz) {
        return -1;
    }
    for (size_t i = 0; i <= n; i++) {
        dst[i] = (char)toupper((unsigned char)src[i]);
    }
    return 0;
}

static int parse_option(dev_options_t *o, const char *opt, const char *val, char *err, size_t esz) {
    char up[32];
    int iv;

    if (strcmp(opt, "--type") == 0) {
        if (to_upper(up, sizeof up, val) != 0 ||
            (o->cfg.type = proto_dev_type_from_str(up)) == DEV_NONE) {
            fail(err, esz, "--type không hợp lệ: %s (dùng sensor, pump, fert hoặc light)", val);
            return -1;
        }
    } else if (strcmp(opt, "--id") == 0) {
        if (!util_valid_id(val, DEVICE_ID_MAX)) {
            fail(err, esz, "--id không hợp lệ: %s (1-16 ký tự chữ, số, '_' hoặc '-')", val);
            return -1;
        }
        util_str_copy(o->cfg.id, sizeof o->cfg.id, val);
    } else if (strcmp(opt, "--port") == 0 || strcmp(opt, "--scan-port") == 0) {
        if (util_parse_int(val, 1, 65535, &iv) != 0) {
            fail(err, esz, "cổng không hợp lệ: %s (phải từ 1 đến 65535)", val);
            return -1;
        }
        if (strcmp(opt, "--port") == 0) {
            o->port = iv;
        } else {
            o->scan_port = iv;
        }
    } else if (strcmp(opt, "--password") == 0) {
        if (util_str_copy(o->cfg.password, sizeof o->cfg.password, val) != 0) {
            fail(err, esz, "--password quá dài%s", "");
            return -1;
        }
    } else if (strcmp(opt, "--timescale") == 0) {
        if (util_parse_double(val, 0.001, 100000.0, &o->cfg.timescale) != 0) {
            fail(err, esz, "--timescale không hợp lệ: %s (từ 0.001 đến 100000)", val);
            return -1;
        }
    } else if (strcmp(opt, "--state-dir") == 0) {
        if (util_str_copy(o->cfg.state_dir, sizeof o->cfg.state_dir, val) != 0) {
            fail(err, esz, "--state-dir quá dài%s", "");
            return -1;
        }
    } else if (strcmp(opt, "--log-dir") == 0) {
        if (util_str_copy(o->cfg.log_dir, sizeof o->cfg.log_dir, val) != 0) {
            fail(err, esz, "--log-dir quá dài%s", "");
            return -1;
        }
    } else if (strcmp(opt, "--bind") == 0) {
        if (util_str_copy(o->bind_ip, sizeof o->bind_ip, val) != 0) {
            fail(err, esz, "--bind quá dài%s", "");
            return -1;
        }
    } else {
        fail(err, esz, "tham số không hợp lệ: %s", opt);
        return -1;
    }
    return 0;
}

int options_parse(int argc, char **argv, dev_options_t *o, char *err, size_t err_sz) {
    if (!o || !argv || argc < 1) {
        fail(err, err_sz, "tham số không hợp lệ%s", "");
        return -1;
    }
    memset(o, 0, sizeof *o);
    o->cfg.timescale = 1.0;
    util_str_copy(o->cfg.state_dir, sizeof o->cfg.state_dir, "data");
    util_str_copy(o->cfg.log_dir, sizeof o->cfg.log_dir, "logs");
    util_str_copy(o->bind_ip, sizeof o->bind_ip, "0.0.0.0");
    o->scan_port = SERVER_SCAN_PORT;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            return 1;
        }
        if (i + 1 >= argc) {
            fail(err, err_sz, "thiếu giá trị cho %s", argv[i]);
            return -1;
        }
        if (parse_option(o, argv[i], argv[i + 1], err, err_sz) != 0) {
            return -1;
        }
        i++;
    }

    if (o->cfg.type == DEV_NONE) {
        fail(err, err_sz, "thiếu --type%s", "");
        return -1;
    }
    if (o->cfg.id[0] == '\0') {
        fail(err, err_sz, "thiếu --id%s", "");
        return -1;
    }
    if (o->port == 0) {
        fail(err, err_sz, "thiếu --port%s", "");
        return -1;
    }
    return 0;
}

void options_usage(FILE *f, const char *prog) {
    fprintf(f,
            "Cách dùng: %s --type sensor|pump|fert|light --id <ID> --port <cổng TCP>\n"
            "           [--password <mật khẩu>] [--timescale <hệ số>]\n"
            "           [--state-dir <thư mục>] [--log-dir <thư mục>]\n"
            "           [--bind <IP>] [--scan-port <cổng UDP>]\n"
            "\n"
            "  --password   bắt buộc ở lần chạy đầu; sau đó mật khẩu lấy từ file trạng thái\n"
            "  --timescale  hệ số tăng tốc đồng hồ giả lập (mặc định 1)\n"
            "  --state-dir  nơi lưu <ID>.state (mặc định data)\n"
            "  --log-dir    nơi lưu <ID>.log (mặc định logs)\n"
            "  --scan-port  cổng UDP nhận lệnh quét (mặc định %d)\n",
            prog ? prog : "device_sim", SERVER_SCAN_PORT);
}
