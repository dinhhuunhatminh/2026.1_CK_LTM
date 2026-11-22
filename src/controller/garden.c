#include "garden.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#define MKDIR(p) mkdir((p), 0755)
#endif

#include "util.h"

#define GARDEN_PATH_MAX 512
#define GARDEN_LINE_MAX 512

static int printable_text(const char *s, size_t max_len, int allow_empty) {
    size_t n;

    if (s == NULL) {
        return allow_empty;
    }
    n = strlen(s);
    if (n == 0) {
        return allow_empty;
    }
    if (n > max_len) {
        return 0;
    }
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c == 0x7f) {
            return 0;
        }
    }
    return 1;
}

static int valid_host_or_token(const char *s, size_t max_len, int allow_empty) {
    if (!printable_text(s, max_len, allow_empty)) {
        return 0;
    }
    if (s == NULL) {
        return 1;
    }
    return strchr(s, ' ') == NULL && strchr(s, '|') == NULL;
}

static int ensure_dir(const char *dir) {
    char tmp[GARDEN_PATH_MAX];
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

void garden_list_init(garden_list_t *list) {
    if (list != NULL) {
        memset(list, 0, sizeof *list);
        list->next_id = 1;
    }
}

static int validate_garden_fields(const char *name, const char *crop,
                                  double area, const char *note) {
    if (!printable_text(name, GARDEN_NAME_MAX, 0) ||
        !printable_text(crop, GARDEN_CROP_MAX, 0) ||
        !printable_text(note, GARDEN_NOTE_MAX, 1) ||
        !isfinite(area) || area <= 0.0) {
        return -1;
    }
    return 0;
}

int garden_add(garden_list_t *list, const char *name, const char *crop,
               double area, const char *note, int *id_out) {
    garden_t *g;

    if (list == NULL || list->count >= GARDEN_MAX_COUNT ||
        validate_garden_fields(name, crop, area, note) != 0) {
        return -1;
    }
    if (list->next_id <= 0) {
        list->next_id = 1;
    }
    g = &list->items[list->count++];
    memset(g, 0, sizeof *g);
    g->id = list->next_id++;
    util_str_copy(g->name, sizeof g->name, name);
    util_str_copy(g->crop, sizeof g->crop, crop);
    g->area = area;
    util_str_copy(g->note, sizeof g->note, note ? note : "");
    if (id_out != NULL) {
        *id_out = g->id;
    }
    return 0;
}

garden_t *garden_find(garden_list_t *list, int garden_id) {
    if (list == NULL || garden_id <= 0) {
        return NULL;
    }
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].id == garden_id) {
            return &list->items[i];
        }
    }
    return NULL;
}

const garden_t *garden_find_const(const garden_list_t *list, int garden_id) {
    if (list == NULL || garden_id <= 0) {
        return NULL;
    }
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].id == garden_id) {
            return &list->items[i];
        }
    }
    return NULL;
}

int garden_update(garden_list_t *list, int garden_id, const char *name,
                  const char *crop, double area, const char *note) {
    garden_t *g = garden_find(list, garden_id);

    if (g == NULL || validate_garden_fields(name, crop, area, note) != 0) {
        return -1;
    }
    util_str_copy(g->name, sizeof g->name, name);
    util_str_copy(g->crop, sizeof g->crop, crop);
    g->area = area;
    util_str_copy(g->note, sizeof g->note, note ? note : "");
    return 0;
}

int garden_remove(garden_list_t *list, int garden_id) {
    if (list == NULL || garden_id <= 0) {
        return -1;
    }
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].id == garden_id) {
            for (int j = i; j < list->count - 1; j++) {
                list->items[j] = list->items[j + 1];
            }
            list->count--;
            return 0;
        }
    }
    return -1;
}

garden_device_t *garden_find_device(garden_t *g, const char *device_id) {
    if (g == NULL || device_id == NULL || device_id[0] == '\0') {
        return NULL;
    }
    for (int i = 0; i < g->device_count; i++) {
        if (strcmp(g->devices[i].device_id, device_id) == 0) {
            return &g->devices[i];
        }
    }
    return NULL;
}

int garden_add_device(garden_t *g, const char *device_id, dev_type_t type,
                      const char *host, int port, const char *token) {
    garden_device_t *d;

    if (g == NULL || g->device_count >= GARDEN_MAX_DEVICES ||
        !util_valid_id(device_id, GARDEN_ID_MAX) || type == DEV_NONE ||
        !valid_host_or_token(host, GARDEN_HOST_MAX, 0) ||
        port <= 0 || port > 65535 ||
        !valid_host_or_token(token, GARDEN_TOKEN_MAX, 1)) {
        return -1;
    }
    if (garden_find_device(g, device_id) != NULL) {
        return -1; /* không trùng device_id trong cùng vườn */
    }
    d = &g->devices[g->device_count++];
    memset(d, 0, sizeof *d);
    util_str_copy(d->device_id, sizeof d->device_id, device_id);
    d->type = type;
    util_str_copy(d->host, sizeof d->host, host);
    d->port = port;
    util_str_copy(d->token, sizeof d->token, token ? token : "");
    return 0;
}

int garden_remove_device(garden_t *g, const char *device_id) {
    if (g == NULL || device_id == NULL) {
        return -1;
    }
    for (int i = 0; i < g->device_count; i++) {
        if (strcmp(g->devices[i].device_id, device_id) == 0) {
            for (int j = i; j < g->device_count - 1; j++) {
                g->devices[j] = g->devices[j + 1];
            }
            g->device_count--;
            return 0;
        }
    }
    return -1;
}

int garden_set_device_token(garden_t *g, const char *device_id, const char *token) {
    garden_device_t *d = garden_find_device(g, device_id);

    if (d == NULL || !valid_host_or_token(token, GARDEN_TOKEN_MAX, 1)) {
        return -1;
    }
    util_str_copy(d->token, sizeof d->token, token ? token : "");
    return 0;
}

int garden_format_list(const garden_list_t *list, char *buf, size_t sz) {
    size_t len = 0;

    if (list == NULL || buf == NULL || sz == 0) {
        return -1;
    }
    if (list->count == 0) {
        int n = snprintf(buf, sz, "(chưa có vườn nào)\n");
        return (n < 0 || (size_t)n >= sz) ? -1 : n;
    }
    for (int i = 0; i < list->count; i++) {
        const garden_t *g = &list->items[i];
        int n = snprintf(buf + len, sz - len,
                         "[#%d] %s | cây: %s | %.1f m2 | %d thiết bị\n",
                         g->id, g->name, g->crop, g->area, g->device_count);
        if (n < 0 || (size_t)n >= sz - len) {
            return -1;
        }
        len += (size_t)n;
    }
    return (int)len;
}

int garden_format_detail(const garden_t *g, char *buf, size_t sz) {
    size_t len = 0;
    int n;

    if (g == NULL || buf == NULL || sz == 0) {
        return -1;
    }
    n = snprintf(buf, sz,
                 "Vườn #%d: %s\nLoại cây: %s\nDiện tích: %.1f m2\nGhi chú: %s\nThiết bị (%d):\n",
                 g->id, g->name, g->crop, g->area, g->note, g->device_count);
    if (n < 0 || (size_t)n >= sz) {
        return -1;
    }
    len = (size_t)n;
    for (int i = 0; i < g->device_count; i++) {
        const garden_device_t *d = &g->devices[i];
        n = snprintf(buf + len, sz - len, "  - %s (%s) %s:%d [%s]\n",
                     d->device_id, proto_dev_type_str(d->type), d->host, d->port,
                     d->token[0] ? "đã có token" : "chưa kết nối");
        if (n < 0 || (size_t)n >= sz - len) {
            return -1;
        }
        len += (size_t)n;
    }
    return (int)len;
}

int garden_save(const garden_list_t *list, const char *dir) {
    char path[GARDEN_PATH_MAX];
    char tmp[GARDEN_PATH_MAX + 8];
    FILE *fp;
    int n;

    if (list == NULL || dir == NULL || dir[0] == '\0') {
        return -1;
    }
    n = snprintf(path, sizeof path, "%s/%s", dir, GARDEN_FILENAME);
    if (n < 0 || (size_t)n >= sizeof path || ensure_dir(dir) != 0) {
        return -1;
    }
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    fp = fopen(tmp, "w");
    if (fp == NULL) {
        return -1;
    }

    fprintf(fp, "# gardens v1\n");
    for (int i = 0; i < list->count; i++) {
        const garden_t *g = &list->items[i];
        fprintf(fp, "[GARDEN]\n");
        fprintf(fp, "id=%d\n", g->id);
        fprintf(fp, "name=%s\n", g->name);
        fprintf(fp, "crop=%s\n", g->crop);
        fprintf(fp, "area=%.6g\n", g->area);
        fprintf(fp, "note=%s\n", g->note);
        for (int j = 0; j < g->device_count; j++) {
            const garden_device_t *d = &g->devices[j];
            fprintf(fp, "device=%s|%s|%s|%d|%s\n",
                    d->device_id, proto_dev_type_str(d->type),
                    d->host, d->port, d->token);
        }
        fprintf(fp, "[/GARDEN]\n");
    }

    if (fflush(fp) != 0 || ferror(fp)) {
        fclose(fp);
        remove(tmp);
        return -1;
    }
    fclose(fp);
    remove(path);
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return -1;
    }
    return 0;
}

/* Phân tích dòng "device=<id>|<type>|<host>|<port>|<token>". */
static int parse_device_line(garden_t *g, char *val) {
    char *parts[5];
    char *p = val;
    int port = 0;
    dev_type_t dt;

    for (int i = 0; i < 4; i++) {
        char *bar = strchr(p, '|');
        if (bar == NULL) {
            return -1;
        }
        *bar = '\0';
        parts[i] = p;
        p = bar + 1;
    }
    parts[4] = p;
    if (strchr(parts[4], '|') != NULL) {
        return -1;
    }
    dt = proto_dev_type_from_str(parts[1]);
    if (util_parse_int(parts[3], 1, 65535, &port) != 0) {
        return -1;
    }
    return garden_add_device(g, parts[0], dt, parts[2], port, parts[4]);
}

int garden_load(garden_list_t *list, const char *dir) {
    char path[GARDEN_PATH_MAX];
    char line[GARDEN_LINE_MAX];
    garden_list_t tmp_list;
    garden_t cur;
    int in_block = 0;
    int has_id = 0, has_name = 0, has_crop = 0, has_area = 0;
    int bad = 0;
    FILE *fp;
    int n;

    if (list == NULL || dir == NULL || dir[0] == '\0') {
        return GARDEN_ERR;
    }
    n = snprintf(path, sizeof path, "%s/%s", dir, GARDEN_FILENAME);
    if (n < 0 || (size_t)n >= sizeof path) {
        return GARDEN_ERR;
    }
    fp = fopen(path, "r");
    if (fp == NULL) {
        if (errno == ENOENT) {
            garden_list_init(list);
            return GARDEN_NOT_FOUND;
        }
        return GARDEN_ERR;
    }

    garden_list_init(&tmp_list);
    memset(&cur, 0, sizeof cur);

    while (!bad && fgets(line, sizeof line, fp) != NULL) {
        size_t len = strlen(line);
        char *p;

        if (len == sizeof(line) - 1 && line[len - 1] != '\n') {
            bad = 1;
            break;
        }
        p = util_trim(line);
        if (*p == '\0' || *p == '#') {
            continue;
        }
        if (strcmp(p, "[GARDEN]") == 0) {
            if (in_block || tmp_list.count >= GARDEN_MAX_COUNT) {
                bad = 1;
                break;
            }
            in_block = 1;
            has_id = has_name = has_crop = has_area = 0;
            memset(&cur, 0, sizeof cur);
            continue;
        }
        if (strcmp(p, "[/GARDEN]") == 0) {
            if (!in_block || !has_id || !has_name || !has_crop || !has_area ||
                validate_garden_fields(cur.name, cur.crop, cur.area, cur.note) != 0 ||
                garden_find(&tmp_list, cur.id) != NULL) {
                bad = 1;
                break;
            }
            tmp_list.items[tmp_list.count++] = cur;
            if (cur.id >= tmp_list.next_id) {
                tmp_list.next_id = cur.id + 1;
            }
            in_block = 0;
            continue;
        }

        if (!in_block) {
            bad = 1;
            break;
        }
        {
            char *eq = strchr(p, '=');
            char *val;
            if (eq == NULL) {
                bad = 1;
                break;
            }
            *eq = '\0';
            val = eq + 1;
            if (strcmp(p, "id") == 0) {
                bad = (util_parse_int(val, 1, 1000000, &cur.id) != 0);
                has_id = 1;
            } else if (strcmp(p, "name") == 0) {
                bad = (util_str_copy(cur.name, sizeof cur.name, val) != 0);
                has_name = 1;
            } else if (strcmp(p, "crop") == 0) {
                bad = (util_str_copy(cur.crop, sizeof cur.crop, val) != 0);
                has_crop = 1;
            } else if (strcmp(p, "area") == 0) {
                bad = (util_parse_double(val, 0.001, 1e9, &cur.area) != 0);
                has_area = 1;
            } else if (strcmp(p, "note") == 0) {
                bad = (util_str_copy(cur.note, sizeof cur.note, val) != 0);
            } else if (strcmp(p, "device") == 0) {
                bad = (parse_device_line(&cur, val) != 0);
            } else {
                bad = 1;
            }
        }
    }
    fclose(fp);

    if (bad || in_block) {
        return GARDEN_ERR;
    }
    *list = tmp_list;
    return GARDEN_OK;
}
