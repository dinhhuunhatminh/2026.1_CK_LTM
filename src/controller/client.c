#include "client.h"

#include <stdio.h>
#include <string.h>

#include "netutil.h"
#include "util.h"

void dev_conn_init(dev_conn_t *conn) {
    if (conn != NULL) {
        memset(conn, 0, sizeof *conn);
        conn->fd = -1;
        conn->timeout_ms = CLIENT_DEFAULT_TIMEOUT_MS;
        linebuf_init(&conn->lb);
    }
}

void dev_close(dev_conn_t *conn) {
    if (conn == NULL) {
        return;
    }
    if (conn->fd >= 0) {
        net_close(conn->fd);
        conn->fd = -1;
    }
    linebuf_init(&conn->lb);
}

int dev_open_with_token(dev_conn_t *conn, const char *host, int port,
                        const char *token, int timeout_ms) {
    int wait_ms = (timeout_ms > 0) ? timeout_ms : CLIENT_DEFAULT_TIMEOUT_MS;
    int fd;

    if (conn == NULL || host == NULL || host[0] == '\0' ||
        port <= 0 || port > 65535 || token == NULL || token[0] == '\0') {
        return -1;
    }
    dev_conn_init(conn);
    if (util_str_copy(conn->host, sizeof conn->host, host) != 0 ||
        util_str_copy(conn->token, sizeof conn->token, token) != 0) {
        return -1;
    }
    fd = net_tcp_connect(host, port, wait_ms);
    if (fd < 0) {
        return -1;
    }
    conn->fd = fd;
    conn->port = port;
    conn->timeout_ms = wait_ms;
    return 0;
}

typedef struct {
    dev_reply_t *rep;
    int          got_header;
    int          want_lines;
    int          bad;
} reply_reader_t;

static void on_reply_line(const char *line, size_t len, int status, void *ctx) {
    reply_reader_t *r = (reply_reader_t *)ctx;
    proto_reply_t pr;

    (void)len;
    if (r->bad) {
        return;
    }
    if (status != LINEBUF_OK) {
        r->bad = 1;
        return;
    }
    if (!r->got_header) {
        if (proto_parse_reply(line, &pr) != 0 ||
            (pr.is_multi && (pr.multi_count < 0 || pr.multi_count > CLIENT_MAX_MULTI_LINES))) {
            r->bad = 1;
            return;
        }
        r->rep->code = pr.code;
        r->rep->is_multi = pr.is_multi;
        util_str_copy(r->rep->multi_tag, sizeof r->rep->multi_tag, pr.multi_tag);
        util_str_copy(r->rep->detail, sizeof r->rep->detail, pr.detail);
        r->rep->line_count = 0;
        r->want_lines = pr.is_multi ? pr.multi_count : 0;
        r->got_header = 1;
        return;
    }

    if (r->rep->line_count < r->want_lines) {
        util_str_copy(r->rep->lines[r->rep->line_count],
                      sizeof r->rep->lines[0], line);
        r->rep->line_count++;
    }
}

int dev_read_reply(dev_conn_t *conn, dev_reply_t *reply) {
    reply_reader_t rr;
    char rbuf[512];

    if (conn == NULL || conn->fd < 0 || reply == NULL) {
        return NET_ERR_IO;
    }
    memset(reply, 0, sizeof *reply);
    memset(&rr, 0, sizeof rr);
    rr.rep = reply;

    while (!rr.bad && (!rr.got_header || reply->line_count < rr.want_lines)) {
        int n = net_recv_timeout(conn->fd, rbuf, sizeof rbuf, conn->timeout_ms);
        if (n == NET_ERR_TIMEOUT) {
            return NET_ERR_TIMEOUT;
        }
        if (n <= 0) {
            return NET_ERR_IO;
        }
        linebuf_feed(&conn->lb, rbuf, (size_t)n, on_reply_line, &rr);
    }

    return rr.bad ? NET_ERR_IO : 0;
}

/* Phân tích phần detail "<token> <device_id> <type> <version>" của phản hồi CONNECT. */
static int parse_connect_detail(dev_conn_t *conn, const char *detail) {
    char tok[PROTO_MAX_TOKEN], id[CLIENT_ID_MAX + 1], tstr[32], ver[CLIENT_VER_MAX + 1], extra[8];
    dev_type_t dt;

    if (sscanf(detail, "%63s %16s %31s %16s %7s", tok, id, tstr, ver, extra) != 4) {
        return -1;
    }
    dt = proto_dev_type_from_str(tstr);
    if (dt == DEV_NONE) {
        return -1;
    }
    util_str_copy(conn->token, sizeof conn->token, tok);
    util_str_copy(conn->device_id, sizeof conn->device_id, id);
    util_str_copy(conn->version, sizeof conn->version, ver);
    conn->type = dt;
    return 0;
}

int dev_connect(dev_conn_t *conn, const char *host, int port,
                const char *app_id, const char *password,
                int timeout_ms, dev_reply_t *reply_out) {
    int wait_ms = (timeout_ms > 0) ? timeout_ms : CLIENT_DEFAULT_TIMEOUT_MS;
    char line[PROTO_MAX_LINE + 2];
    dev_reply_t local_rep;
    dev_reply_t *rep = reply_out ? reply_out : &local_rep;
    int n;

    if (conn == NULL || host == NULL || host[0] == '\0' ||
        port <= 0 || port > 65535 ||
        app_id == NULL || app_id[0] == '\0' || strchr(app_id, ' ') != NULL ||
        password == NULL || password[0] == '\0' || strchr(password, ' ') != NULL) {
        return -1;
    }
    dev_conn_init(conn);
    if (util_str_copy(conn->host, sizeof conn->host, host) != 0) {
        return -1;
    }
    n = snprintf(line, sizeof line, "CONNECT %s %s\n", app_id, password);
    conn->fd = net_tcp_connect(host, port, wait_ms);
    if (conn->fd < 0) {
        return -1;
    }
    conn->port = port;
    conn->timeout_ms = wait_ms;

    if (net_send_all(conn->fd, line, (size_t)n) != 0 ||
        dev_read_reply(conn, rep) != 0) {
        dev_close(conn);
        return -1;
    }
    if (rep->code != PROTO_OK) {
        dev_close(conn);
        return rep->code;
    }
    if (parse_connect_detail(conn, rep->detail) != 0) {
        dev_close(conn);
        return -1;
    }
    return PROTO_OK;
}

int dev_send_cmd(dev_conn_t *conn, const char *cmd_line, dev_reply_t *reply) {
    char line[PROTO_MAX_LINE + 2];
    int n;

    if (conn == NULL || conn->fd < 0 || conn->token[0] == '\0' ||
        cmd_line == NULL || cmd_line[0] == '\0' || reply == NULL) {
        return -1;
    }
    n = snprintf(line, sizeof line, "%s %s\n", conn->token, cmd_line);
    if (n < 0 || (size_t)n >= sizeof line ||
        net_send_all(conn->fd, line, (size_t)n) != 0 ||
        dev_read_reply(conn, reply) != 0) {
        return -1;
    }
    return reply->code;
}

const char *dev_strerror_vi(int code) {
    return proto_code_vi(code);
}
