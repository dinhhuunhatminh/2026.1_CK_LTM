/*
 * options.h - phân tích tham số dòng lệnh của device_sim.
 *
 *   device_sim --type sensor|pump|fert|light --id <ID> --port <cổng TCP>
 *              [--password <mật khẩu>] [--timescale <hệ số>]
 *              [--state-dir <thư mục>] [--log-dir <thư mục>]
 *              [--bind <IP>] [--scan-port <cổng UDP>]
 */
#ifndef OPTIONS_H
#define OPTIONS_H

#include <stddef.h>
#include <stdio.h>

#include "device.h"

#define OPTIONS_BIND_MAX 64

typedef struct {
    device_config_t cfg;
    int             port;      /* cổng TCP */
    int             scan_port; /* cổng UDP quét, mặc định 9999 */
    char            bind_ip[OPTIONS_BIND_MAX];
} dev_options_t;

/*
 * Phân tích argv. Trả về:
 *   0  thành công, o đã điền (kèm giá trị mặc định cho tham số không có)
 *   1  người dùng yêu cầu --help / -h
 *  -1  lỗi; thông báo tiếng Việt nằm trong err
 */
int options_parse(int argc, char **argv, dev_options_t *o, char *err, size_t err_sz);

/* In hướng dẫn sử dụng ra f. */
void options_usage(FILE *f, const char *prog);

#endif /* OPTIONS_H */
