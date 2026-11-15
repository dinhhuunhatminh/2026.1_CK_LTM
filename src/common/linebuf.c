#include "linebuf.h"

#include <string.h>

void linebuf_init(linebuf_t *lb) {
    if (lb == NULL) {
        return;
    }
    memset(lb, 0, sizeof *lb);
}

size_t linebuf_pending(const linebuf_t *lb) {
    if (lb == NULL) {
        return 0;
    }
    return lb->len;
}

/* Gọi callback cho dòng vừa kết thúc rồi dọn bộ đệm cho dòng kế tiếp. */
static void finish_line(linebuf_t *lb, linebuf_cb cb, void *ctx) {
    if (lb->too_long) {
        cb("", 0, LINEBUF_TOO_LONG, ctx);
    } else if (lb->bad_byte) {
        cb("", 0, LINEBUF_BAD_BYTE, ctx);
    } else {
        size_t len = lb->len;
        if (len > 0 && lb->data[len - 1] == '\r') {
            len--;
        }
        if (len > LINEBUF_MAX_LINE) {
            cb("", 0, LINEBUF_TOO_LONG, ctx);
        } else {
            lb->data[len] = '\0';
            cb(lb->data, len, LINEBUF_OK, ctx);
        }
    }
    lb->len = 0;
    lb->too_long = 0;
    lb->bad_byte = 0;
}

void linebuf_feed(linebuf_t *lb, const char *data, size_t n, linebuf_cb cb, void *ctx) {
    size_t i;

    if (lb == NULL || data == NULL || cb == NULL) {
        return;
    }
    for (i = 0; i < n; i++) {
        char c = data[i];

        if (c == '\n') {
            finish_line(lb, cb, ctx);
        } else if (lb->too_long) {
            /* đang bỏ phần thừa đến hết dòng */
        } else if (c == '\0') {
            lb->bad_byte = 1;
        } else if (lb->len < LINEBUF_MAX_LINE + 1) {
            /* +1 để chừa chỗ cho '\r' của "\r\n" sau dòng dài đúng MAX_LINE */
            lb->data[lb->len++] = c;
        } else {
            lb->too_long = 1;
            lb->len = 0;
        }
    }
}
