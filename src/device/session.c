#include "session.h"

#include <string.h>

#include "proto.h"
#include "util.h"

void session_init(session_t *s) {
    if (s) {
        memset(s, 0, sizeof *s);
    }
}

static session_fail_t *find_fail(session_t *s, const char *app_id) {
    for (int i = 0; i < SESSION_MAX_TRACK; i++) {
        if (s->fails[i].used && strcmp(s->fails[i].app_id, app_id) == 0) {
            return &s->fails[i];
        }
    }
    return NULL;
}

/* Lấy bản ghi đếm lỗi của app_id; chưa có thì tạo, hết chỗ thì thay bản ghi cũ nhất. */
static session_fail_t *alloc_fail(session_t *s, const char *app_id) {
    session_fail_t *slot = NULL;
    for (int i = 0; i < SESSION_MAX_TRACK; i++) {
        if (!s->fails[i].used) {
            slot = &s->fails[i];
            break;
        }
        if (slot == NULL || s->fails[i].last_sec < slot->last_sec) {
            slot = &s->fails[i];
        }
    }
    memset(slot, 0, sizeof *slot);
    slot->used = 1;
    util_str_copy(slot->app_id, sizeof slot->app_id, app_id);
    return slot;
}

static session_app_t *find_app(session_t *s, const char *app_id) {
    for (int i = 0; i < SESSION_MAX_APPS; i++) {
        if (s->apps[i].used && strcmp(s->apps[i].app_id, app_id) == 0) {
            return &s->apps[i];
        }
    }
    return NULL;
}

static int token_in_use(const session_t *s, const char *token) {
    for (int i = 0; i < SESSION_MAX_APPS; i++) {
        if (s->apps[i].used && strcmp(s->apps[i].token, token) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Sinh token chưa trùng với token nào đang dùng. */
static int new_token(const session_t *s, char *out) {
    for (int tries = 0; tries < 8; tries++) {
        if (util_random_hex(out, SESSION_TOKEN_LEN) != 0) {
            return -1;
        }
        if (!token_in_use(s, out)) {
            return 0;
        }
    }
    return -1;
}

int session_connect(session_t *s, const char *app_id, int password_ok,
                    int64_t now_sec, char *token_out) {
    if (!s || !app_id || !token_out || !util_valid_id(app_id, SESSION_ID_MAX)) {
        return PROTO_BAD_ARG;
    }

    session_fail_t *f = find_fail(s, app_id);
    if (f && f->locked_until != 0) {
        if (now_sec < f->locked_until) {
            return PROTO_LOCKED;
        }
        f->locked_until = 0; /* hết hạn khóa: cho thử lại từ đầu */
        f->fails = 0;
    }

    if (!password_ok) {
        if (!f) {
            f = alloc_fail(s, app_id);
        }
        f->fails++;
        f->last_sec = now_sec;
        if (f->fails >= SESSION_MAX_FAILS) {
            f->locked_until = now_sec + SESSION_LOCK_SEC;
            f->fails = 0;
        }
        return PROTO_BAD_PASSWORD;
    }

    if (f) {
        f->used = 0; /* nhập đúng: xóa bộ đếm lỗi */
    }

    session_app_t *a = find_app(s, app_id);
    if (a) {
        util_str_copy(token_out, SESSION_TOKEN_LEN + 1, a->token);
        return PROTO_OK;
    }

    for (int i = 0; i < SESSION_MAX_APPS; i++) {
        if (!s->apps[i].used) {
            char tok[SESSION_TOKEN_LEN + 1];
            if (new_token(s, tok) != 0) {
                return PROTO_UNKNOWN_ERR;
            }
            s->apps[i].used = 1;
            util_str_copy(s->apps[i].app_id, sizeof s->apps[i].app_id, app_id);
            util_str_copy(s->apps[i].token, sizeof s->apps[i].token, tok);
            util_str_copy(token_out, SESSION_TOKEN_LEN + 1, tok);
            return PROTO_OK;
        }
    }
    return PROTO_TOO_MANY_APPS;
}

const char *session_validate(const session_t *s, const char *token) {
    if (!s || !token || !util_valid_hex(token, SESSION_TOKEN_LEN)) {
        return NULL;
    }
    for (int i = 0; i < SESSION_MAX_APPS; i++) {
        if (s->apps[i].used && strcmp(s->apps[i].token, token) == 0) {
            return s->apps[i].app_id;
        }
    }
    return NULL;
}

int session_is_locked(const session_t *s, const char *app_id, int64_t now_sec) {
    if (!s || !app_id) {
        return 0;
    }
    for (int i = 0; i < SESSION_MAX_TRACK; i++) {
        if (s->fails[i].used && strcmp(s->fails[i].app_id, app_id) == 0) {
            return s->fails[i].locked_until != 0 && now_sec < s->fails[i].locked_until;
        }
    }
    return 0;
}

int session_revoke_others(session_t *s, const char *keep_token) {
    int n = 0;
    if (!s) {
        return 0;
    }
    for (int i = 0; i < SESSION_MAX_APPS; i++) {
        if (s->apps[i].used &&
            (keep_token == NULL || strcmp(s->apps[i].token, keep_token) != 0)) {
            memset(&s->apps[i], 0, sizeof s->apps[i]);
            n++;
        }
    }
    return n;
}

int session_count(const session_t *s) {
    int n = 0;
    if (!s) {
        return 0;
    }
    for (int i = 0; i < SESSION_MAX_APPS; i++) {
        n += s->apps[i].used ? 1 : 0;
    }
    return n;
}
