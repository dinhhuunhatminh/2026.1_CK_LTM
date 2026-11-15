/* Unit test cho linebuf: xử lý truyền dòng trên TCP stream. */
#include <string.h>

#include "linebuf.h"
#include "minitest.h"

#define MAX_REC 16

/* Gom lại các dòng mà callback nhận được để kiểm tra. */
typedef struct {
    int    count;
    char   line[MAX_REC][LINEBUF_MAX_LINE + 1];
    size_t len[MAX_REC];
    int    status[MAX_REC];
} recorder_t;

static void record(const char *line, size_t len, int status, void *ctx) {
    recorder_t *r = ctx;
    if (r->count < MAX_REC) {
        strcpy(r->line[r->count], line);
        r->len[r->count] = len;
        r->status[r->count] = status;
    }
    r->count++;
}

static void feed_str(linebuf_t *lb, recorder_t *r, const char *s) {
    linebuf_feed(lb, s, strlen(s), record, r);
}

/* Tạo chuỗi gồm n ký tự 'a' (buf phải đủ n+1 byte). */
static void make_a(char *buf, size_t n) {
    memset(buf, 'a', n);
    buf[n] = '\0';
}

static void test_one_complete_line(void) {
    linebuf_t lb; recorder_t r = {0};
    linebuf_init(&lb);
    feed_str(&lb, &r, "CONNECT abc 1234\n");
    ASSERT_EQ_INT(1, r.count);
    ASSERT_STR_EQ("CONNECT abc 1234", r.line[0]);
    ASSERT_EQ_INT(16, r.len[0]);
    ASSERT_EQ_INT(LINEBUF_OK, r.status[0]);
    ASSERT_EQ_INT(0, linebuf_pending(&lb));
}

static void test_line_split_across_feeds(void) {
    linebuf_t lb; recorder_t r = {0};
    linebuf_init(&lb);
    feed_str(&lb, &r, "SET HM");
    ASSERT_EQ_INT(0, r.count);
    ASSERT_EQ_INT(6, linebuf_pending(&lb));
    feed_str(&lb, &r, "IN 30");
    ASSERT_EQ_INT(0, r.count);
    feed_str(&lb, &r, "\n");
    ASSERT_EQ_INT(1, r.count);
    ASSERT_STR_EQ("SET HMIN 30", r.line[0]);
    ASSERT_EQ_INT(0, linebuf_pending(&lb));
}

static void test_multiple_lines_in_one_chunk(void) {
    linebuf_t lb; recorder_t r = {0};
    linebuf_init(&lb);
    feed_str(&lb, &r, "INFO\nSTATUS\nPARAMS\n");
    ASSERT_EQ_INT(3, r.count);
    ASSERT_STR_EQ("INFO", r.line[0]);
    ASSERT_STR_EQ("STATUS", r.line[1]);
    ASSERT_STR_EQ("PARAMS", r.line[2]);
}

static void test_complete_lines_plus_partial_tail(void) {
    linebuf_t lb; recorder_t r = {0};
    linebuf_init(&lb);
    feed_str(&lb, &r, "INFO\nSTAT");
    ASSERT_EQ_INT(1, r.count);
    ASSERT_EQ_INT(4, linebuf_pending(&lb));
    feed_str(&lb, &r, "US\n");
    ASSERT_EQ_INT(2, r.count);
    ASSERT_STR_EQ("STATUS", r.line[1]);
}

static void test_crlf_is_stripped(void) {
    linebuf_t lb; recorder_t r = {0};
    linebuf_init(&lb);
    feed_str(&lb, &r, "INFO\r\nSTATUS\r\n");
    ASSERT_EQ_INT(2, r.count);
    ASSERT_STR_EQ("INFO", r.line[0]);
    ASSERT_EQ_INT(4, r.len[0]);
    ASSERT_STR_EQ("STATUS", r.line[1]);
}

static void test_cr_and_lf_in_separate_feeds(void) {
    linebuf_t lb; recorder_t r = {0};
    linebuf_init(&lb);
    feed_str(&lb, &r, "INFO\r");
    ASSERT_EQ_INT(0, r.count);
    feed_str(&lb, &r, "\n");
    ASSERT_EQ_INT(1, r.count);
    ASSERT_STR_EQ("INFO", r.line[0]);
}

static void test_lone_cr_in_middle_is_kept(void) {
    linebuf_t lb; recorder_t r = {0};
    linebuf_init(&lb);
    feed_str(&lb, &r, "a\rb\n");
    ASSERT_EQ_INT(1, r.count);
    ASSERT_STR_EQ("a\rb", r.line[0]);
    ASSERT_EQ_INT(3, r.len[0]);
}

static void test_byte_by_byte(void) {
    linebuf_t lb; recorder_t r = {0};
    const char *msg = "POWER ON\nPASSWD a b\n";
    size_t i;
    linebuf_init(&lb);
    for (i = 0; i < strlen(msg); i++) {
        linebuf_feed(&lb, msg + i, 1, record, &r);
    }
    ASSERT_EQ_INT(2, r.count);
    ASSERT_STR_EQ("POWER ON", r.line[0]);
    ASSERT_STR_EQ("PASSWD a b", r.line[1]);
}

static void test_empty_line(void) {
    linebuf_t lb; recorder_t r = {0};
    linebuf_init(&lb);
    feed_str(&lb, &r, "\n\r\n");
    ASSERT_EQ_INT(2, r.count);
    ASSERT_EQ_INT(0, r.len[0]);
    ASSERT_STR_EQ("", r.line[0]);
    ASSERT_EQ_INT(0, r.len[1]);
    ASSERT_EQ_INT(LINEBUF_OK, r.status[1]);
}

static void test_zero_length_feed_is_noop(void) {
    linebuf_t lb; recorder_t r = {0};
    linebuf_init(&lb);
    linebuf_feed(&lb, "abc", 0, record, &r);
    ASSERT_EQ_INT(0, r.count);
    ASSERT_EQ_INT(0, linebuf_pending(&lb));
}

static void test_max_length_line_ok(void) {
    linebuf_t lb; recorder_t r = {0};
    char big[LINEBUF_MAX_LINE + 4];
    linebuf_init(&lb);
    make_a(big, LINEBUF_MAX_LINE);
    feed_str(&lb, &r, big);
    feed_str(&lb, &r, "\n");
    ASSERT_EQ_INT(1, r.count);
    ASSERT_EQ_INT(LINEBUF_OK, r.status[0]);
    ASSERT_EQ_INT(LINEBUF_MAX_LINE, r.len[0]);
    ASSERT_STR_EQ(big, r.line[0]);
}

static void test_max_length_line_with_crlf_ok(void) {
    linebuf_t lb; recorder_t r = {0};
    char big[LINEBUF_MAX_LINE + 4];
    linebuf_init(&lb);
    make_a(big, LINEBUF_MAX_LINE);
    feed_str(&lb, &r, big);
    feed_str(&lb, &r, "\r\n");
    ASSERT_EQ_INT(1, r.count);
    ASSERT_EQ_INT(LINEBUF_OK, r.status[0]);
    ASSERT_EQ_INT(LINEBUF_MAX_LINE, r.len[0]);
}

static void test_one_over_max_is_too_long(void) {
    linebuf_t lb; recorder_t r = {0};
    char big[LINEBUF_MAX_LINE + 4];
    linebuf_init(&lb);
    make_a(big, LINEBUF_MAX_LINE + 1);
    feed_str(&lb, &r, big);
    feed_str(&lb, &r, "\n");
    ASSERT_EQ_INT(1, r.count);
    ASSERT_EQ_INT(LINEBUF_TOO_LONG, r.status[0]);
    ASSERT_EQ_INT(0, r.len[0]);
    ASSERT_STR_EQ("", r.line[0]);
}

static void test_one_over_max_with_crlf_is_too_long(void) {
    linebuf_t lb; recorder_t r = {0};
    char big[LINEBUF_MAX_LINE + 4];
    linebuf_init(&lb);
    make_a(big, LINEBUF_MAX_LINE + 1);
    feed_str(&lb, &r, big);
    feed_str(&lb, &r, "\r\n");
    ASSERT_EQ_INT(1, r.count);
    ASSERT_EQ_INT(LINEBUF_TOO_LONG, r.status[0]);
}

static void test_too_long_reported_once_then_recovers(void) {
    linebuf_t lb; recorder_t r = {0};
    char big[LINEBUF_MAX_LINE * 3];
    linebuf_init(&lb);
    make_a(big, LINEBUF_MAX_LINE * 2 + 100);
    /* dòng rất dài đến trong 3 mảnh, sau đó là một dòng bình thường */
    linebuf_feed(&lb, big, LINEBUF_MAX_LINE, record, &r);
    linebuf_feed(&lb, big, LINEBUF_MAX_LINE, record, &r);
    linebuf_feed(&lb, big, 100, record, &r);
    ASSERT_EQ_INT(0, r.count);
    feed_str(&lb, &r, "\nINFO\n");
    ASSERT_EQ_INT(2, r.count);
    ASSERT_EQ_INT(LINEBUF_TOO_LONG, r.status[0]);
    ASSERT_EQ_INT(LINEBUF_OK, r.status[1]);
    ASSERT_STR_EQ("INFO", r.line[1]);
    ASSERT_EQ_INT(0, linebuf_pending(&lb));
}

static void test_nul_byte_rejected_then_recovers(void) {
    linebuf_t lb; recorder_t r = {0};
    linebuf_init(&lb);
    linebuf_feed(&lb, "AB\0CD\nINFO\n", 11, record, &r);
    ASSERT_EQ_INT(2, r.count);
    ASSERT_EQ_INT(LINEBUF_BAD_BYTE, r.status[0]);
    ASSERT_STR_EQ("", r.line[0]);
    ASSERT_EQ_INT(LINEBUF_OK, r.status[1]);
    ASSERT_STR_EQ("INFO", r.line[1]);
}

static void test_too_long_wins_over_nul(void) {
    linebuf_t lb; recorder_t r = {0};
    char big[LINEBUF_MAX_LINE + 10];
    linebuf_init(&lb);
    make_a(big, LINEBUF_MAX_LINE + 5);
    feed_str(&lb, &r, big);
    linebuf_feed(&lb, "\0\n", 2, record, &r);
    ASSERT_EQ_INT(1, r.count);
    ASSERT_EQ_INT(LINEBUF_TOO_LONG, r.status[0]);
}

static void test_null_arguments_are_ignored(void) {
    linebuf_t lb; recorder_t r = {0};
    linebuf_init(&lb);
    linebuf_init(NULL);
    linebuf_feed(NULL, "a\n", 2, record, &r);
    linebuf_feed(&lb, NULL, 2, record, &r);
    linebuf_feed(&lb, "a\n", 2, NULL, &r);
    ASSERT_EQ_INT(0, r.count);
    ASSERT_EQ_INT(0, linebuf_pending(NULL));
}

static void test_init_clears_pending_data(void) {
    linebuf_t lb; recorder_t r = {0};
    linebuf_init(&lb);
    feed_str(&lb, &r, "abc");
    ASSERT_EQ_INT(3, linebuf_pending(&lb));
    linebuf_init(&lb);
    ASSERT_EQ_INT(0, linebuf_pending(&lb));
    feed_str(&lb, &r, "x\n");
    ASSERT_STR_EQ("x", r.line[0]);
}

int main(void) {
    printf("test_linebuf\n");
    RUN_TEST(test_one_complete_line);
    RUN_TEST(test_line_split_across_feeds);
    RUN_TEST(test_multiple_lines_in_one_chunk);
    RUN_TEST(test_complete_lines_plus_partial_tail);
    RUN_TEST(test_crlf_is_stripped);
    RUN_TEST(test_cr_and_lf_in_separate_feeds);
    RUN_TEST(test_lone_cr_in_middle_is_kept);
    RUN_TEST(test_byte_by_byte);
    RUN_TEST(test_empty_line);
    RUN_TEST(test_zero_length_feed_is_noop);
    RUN_TEST(test_max_length_line_ok);
    RUN_TEST(test_max_length_line_with_crlf_ok);
    RUN_TEST(test_one_over_max_is_too_long);
    RUN_TEST(test_one_over_max_with_crlf_is_too_long);
    RUN_TEST(test_too_long_reported_once_then_recovers);
    RUN_TEST(test_nul_byte_rejected_then_recovers);
    RUN_TEST(test_too_long_wins_over_nul);
    RUN_TEST(test_null_arguments_are_ignored);
    RUN_TEST(test_init_clears_pending_data);
    return MT_SUMMARY();
}
