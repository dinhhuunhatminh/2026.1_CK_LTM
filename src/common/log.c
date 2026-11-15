#include "log.h"
#include "simclock.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *log_source_str(log_source_t src) {
    switch (src) {
    case LOG_SRC_AUTH: return "AUTH";
    case LOG_SRC_CMD:  return "CMD";
    case LOG_SRC_AUTO: return "AUTO";
    case LOG_SRC_SYS:  return "SYS";
    case LOG_SRC_WARN: return "WARN";
    default:           return "SYS";
    }
}

int log_init(logger_t *lg, const char *path) {
    size_t len;

    if (lg == NULL || path == NULL || path[0] == '\0') {
        return -1;
    }
    len = strlen(path);
    if (len >= LOG_MAX_PATH) {
        return -1;
    }
    memcpy(lg->path, path, len + 1);
    return 0;
}

int log_write(const logger_t *lg, int64_t sim_sec,
              log_source_t src, const char *fmt, ...) {
    char ts[32];
    char msg[LOG_MAX_LINE];
    va_list ap;
    FILE *fp;
    int n;

    if (lg == NULL || lg->path[0] == '\0' || fmt == NULL) {
        return -1;
    }
    if (simclock_format_time(sim_sec < 0 ? 0 : sim_sec, ts, sizeof ts) != 0) {
        return -1;
    }

    va_start(ap, fmt);
    n = vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    if (n < 0) {
        return -1;
    }

    fp = fopen(lg->path, "ab");
    if (fp == NULL) {
        return -1;
    }
    fprintf(fp, "%s %s %s\n", ts, log_source_str(src), msg);
    fclose(fp);
    return 0;
}

int log_tail(const logger_t *lg, char lines[][LOG_MAX_LINE], int max_lines) {
    FILE *fp;
    char buf[LOG_MAX_LINE];
    int total = 0;
    int count;
    int start;
    int i;

    if (lg == NULL || lg->path[0] == '\0' || lines == NULL || max_lines <= 0) {
        return -1;
    }
    fp = fopen(lg->path, "rb");
    if (fp == NULL) {
        return 0;
    }

    /* Đọc vòng tròn vào mảng lines[0..max_lines-1] */
    while (fgets(buf, sizeof buf, fp) != NULL) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            len--;
        }
        buf[len] = '\0';
        strcpy(lines[total % max_lines], buf);
        total++;
    }
    fclose(fp);

    if (total <= max_lines) {
        return total;
    }

    /* Sắp xếp lại mảng vòng để dòng cũ nhất đứng ở vị trí 0 */
    count = max_lines;
    start = total % max_lines;
    if (start != 0) {
        char (*tmp)[LOG_MAX_LINE] = malloc((size_t)count * LOG_MAX_LINE);
        if (tmp != NULL) {
            for (i = 0; i < count; i++) {
                strcpy(tmp[i], lines[(start + i) % count]);
            }
            for (i = 0; i < count; i++) {
                strcpy(lines[i], tmp[i]);
            }
            free(tmp);
        }
    }
    return count;
}
