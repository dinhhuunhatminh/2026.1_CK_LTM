#include "store.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"

#define DEVICE_ID_MAX 16
#define PATH_MAX_LEN  512

void store_init(store_t *s) {
    if (s) {
        memset(s, 0, sizeof *s);
    }
}

static int valid_key(const char *k) {
    size_t n;
    if (!k) {
        return 0;
    }
    n = strlen(k);
    if (n == 0 || n > STORE_KEY_MAX) {
        return 0;
    }
    for (size_t i = 0; i < n; i++) {
        char c = k[i];
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
        if (!ok) {
            return 0;
        }
    }
    return 1;
}

/* Ký tự in được (kể cả dấu cách), không có xuống dòng hay ký tự điều khiển. */
static int printable_only(const char *v) {
    for (; *v; v++) {
        unsigned char c = (unsigned char)*v;
        if (c < 0x20 || c == 0x7f) {
            return 0;
        }
    }
    return 1;
}

int store_set_password(store_t *s, const char *password) {
    size_t n;
    if (!s || !password) {
        return -1;
    }
    n = strlen(password);
    if (n < STORE_PASS_MIN || n > STORE_PASS_MAX || !printable_only(password) ||
        strchr(password, ' ') != NULL) {
        return -1;
    }
    memcpy(s->password, password, n + 1);
    return 0;
}

int store_password_matches(const store_t *s, const char *password) {
    if (!s || !password || s->password[0] == '\0') {
        return 0;
    }
    return strcmp(s->password, password) == 0;
}

int store_set(store_t *s, const char *key, const char *value) {
    if (!s || !valid_key(key) || !value || strcmp(key, "password") == 0 ||
        strlen(value) > STORE_VAL_MAX || !printable_only(value)) {
        return -1;
    }
    for (int i = 0; i < s->count; i++) {
        if (strcmp(s->kv[i].key, key) == 0) {
            memcpy(s->kv[i].value, value, strlen(value) + 1);
            return 0;
        }
    }
    if (s->count >= STORE_MAX_KV) {
        return -1;
    }
    memcpy(s->kv[s->count].key, key, strlen(key) + 1);
    memcpy(s->kv[s->count].value, value, strlen(value) + 1);
    s->count++;
    return 0;
}

const char *store_get(const store_t *s, const char *key) {
    if (!s || !key) {
        return NULL;
    }
    for (int i = 0; i < s->count; i++) {
        if (strcmp(s->kv[i].key, key) == 0) {
            return s->kv[i].value;
        }
    }
    return NULL;
}

static int build_path(char *buf, size_t sz, const char *dir, const char *id) {
    int n;
    if (!dir || !id || dir[0] == '\0' || !util_valid_id(id, DEVICE_ID_MAX)) {
        return -1;
    }
    n = snprintf(buf, sz, "%s/%s.state", dir, id);
    return (n < 0 || (size_t)n >= sz) ? -1 : 0;
}

int store_save(const store_t *s, const char *dir, const char *device_id) {
    char path[PATH_MAX_LEN], tmp[PATH_MAX_LEN + 8];
    FILE *f;
    int fd;

    if (!s || s->password[0] == '\0' || build_path(path, sizeof path, dir, device_id) != 0) {
        return -1;
    }
    if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
        return -1;
    }
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return -1;
    }
    f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        unlink(tmp);
        return -1;
    }
    fprintf(f, "# device state v1\npassword=%s\n", s->password);
    for (int i = 0; i < s->count; i++) {
        fprintf(f, "%s=%s\n", s->kv[i].key, s->kv[i].value);
    }
    if (fflush(f) != 0 || ferror(f)) {
        fclose(f);
        unlink(tmp);
        return -1;
    }
    if (fclose(f) != 0 || rename(tmp, path) != 0) {
        unlink(tmp);
        return -1;
    }
    return 0;
}

int store_load(store_t *s, const char *dir, const char *device_id) {
    char path[PATH_MAX_LEN], line[STORE_LINE_MAX];
    store_t t;
    FILE *f;
    int bad = 0;

    if (!s || build_path(path, sizeof path, dir, device_id) != 0) {
        return STORE_ERR;
    }
    f = fopen(path, "r");
    if (!f) {
        return errno == ENOENT ? STORE_NOT_FOUND : STORE_ERR;
    }

    store_init(&t);
    while (!bad && fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        char *p, *eq;
        if (n == sizeof line - 1 && line[n - 1] != '\n') {
            bad = 1; /* dòng dài quá giới hạn */
            break;
        }
        p = util_trim(line);
        if (*p == '\0' || *p == '#') {
            continue;
        }
        eq = strchr(p, '=');
        if (!eq) {
            bad = 1;
            break;
        }
        *eq = '\0';
        if (strcmp(p, "password") == 0) {
            bad = store_set_password(&t, eq + 1) != 0;
        } else {
            bad = store_set(&t, p, eq + 1) != 0;
        }
    }
    if (ferror(f)) {
        bad = 1;
    }
    fclose(f);

    if (bad || t.password[0] == '\0') {
        return STORE_ERR;
    }
    *s = t;
    return STORE_OK;
}
