#include "appid.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir((p), 0755)
#endif

#include "util.h"

#define APPID_PATH_MAX 512

/* Tạo thư mục (bao gồm cả thư mục cha nếu chưa có). */
static int ensure_dir_recursive(const char *dir) {
    char tmp[APPID_PATH_MAX];
    size_t len = strlen(dir);

    if (len == 0 || len >= sizeof tmp) {
        return -1;
    }
    memcpy(tmp, dir, len + 1);
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char saved = tmp[i];
            tmp[i] = '\0';
            (void)MKDIR(tmp);
            tmp[i] = saved;
        }
    }
    (void)MKDIR(tmp);
    return 0;
}

static int try_read_existing(const char *path, char *out_id, size_t out_sz) {
    FILE *fp = fopen(path, "r");
    char buf[64];

    if (fp == NULL) {
        return -1;
    }
    if (fgets(buf, sizeof buf, fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    util_trim(buf);
    if (!util_valid_hex(buf, APPID_LEN)) {
        return -1;
    }
    return util_str_copy(out_id, out_sz, buf);
}

int appid_get(const char *dir, char *out_id, size_t out_sz) {
    char path[APPID_PATH_MAX];
    char id[APPID_BUF_SIZE];
    FILE *fp;
    int n;

    if (dir == NULL || dir[0] == '\0' || out_id == NULL || out_sz < APPID_BUF_SIZE) {
        return -1;
    }
    n = snprintf(path, sizeof path, "%s/%s", dir, APPID_FILENAME);
    if (n < 0 || (size_t)n >= sizeof path) {
        return -1;
    }
    if (try_read_existing(path, out_id, out_sz) == 0) {
        return 0;
    }
    if (ensure_dir_recursive(dir) != 0 || util_random_hex(id, APPID_LEN) != 0) {
        return -1;
    }
    fp = fopen(path, "w");
    if (fp == NULL) {
        return -1;
    }
    fprintf(fp, "%s\n", id);
    fclose(fp);
    return util_str_copy(out_id, out_sz, id);
}
