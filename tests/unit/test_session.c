/* Unit test cho session: token, giới hạn ứng dụng, khóa chống dò mật khẩu. */
#include <stdio.h>
#include <string.h>

#include "minitest.h"
#include "proto.h"
#include "session.h"
#include "util.h"

#define APP_A "aaaaaaaaaaaaaaaa"
#define APP_B "bbbbbbbbbbbbbbbb"

static void test_connect_ok_returns_valid_token(void) {
    session_t s; char tok[SESSION_TOKEN_LEN + 1];
    session_init(&s);
    ASSERT_EQ_INT(PROTO_OK, session_connect(&s, APP_A, 1, 100, tok));
    ASSERT_EQ_INT(SESSION_TOKEN_LEN, strlen(tok));
    ASSERT_TRUE(util_valid_hex(tok, SESSION_TOKEN_LEN));
    ASSERT_EQ_INT(1, session_count(&s));
    ASSERT_STR_EQ(APP_A, session_validate(&s, tok));
}

static void test_reconnect_returns_same_token(void) {
    session_t s; char t1[SESSION_TOKEN_LEN + 1], t2[SESSION_TOKEN_LEN + 1];
    session_init(&s);
    ASSERT_EQ_INT(PROTO_OK, session_connect(&s, APP_A, 1, 0, t1));
    ASSERT_EQ_INT(PROTO_OK, session_connect(&s, APP_A, 1, 5, t2));
    ASSERT_STR_EQ(t1, t2);
    ASSERT_EQ_INT(1, session_count(&s));
}

static void test_two_apps_get_different_tokens(void) {
    session_t s; char ta[SESSION_TOKEN_LEN + 1], tb[SESSION_TOKEN_LEN + 1];
    session_init(&s);
    ASSERT_EQ_INT(PROTO_OK, session_connect(&s, APP_A, 1, 0, ta));
    ASSERT_EQ_INT(PROTO_OK, session_connect(&s, APP_B, 1, 0, tb));
    ASSERT_TRUE(strcmp(ta, tb) != 0);
    ASSERT_STR_EQ(APP_A, session_validate(&s, ta));
    ASSERT_STR_EQ(APP_B, session_validate(&s, tb));
    ASSERT_EQ_INT(2, session_count(&s));
}

static void test_validate_rejects_bad_tokens(void) {
    session_t s; char tok[SESSION_TOKEN_LEN + 1];
    session_init(&s);
    session_connect(&s, APP_A, 1, 0, tok);
    ASSERT_NULL(session_validate(&s, NULL));
    ASSERT_NULL(session_validate(NULL, tok));
    ASSERT_NULL(session_validate(&s, ""));
    ASSERT_NULL(session_validate(&s, "abc"));                  /* quá ngắn */
    ASSERT_NULL(session_validate(&s, "zzzzzzzzzzzzzzzz"));     /* không phải hex */
    ASSERT_NULL(session_validate(&s, "0000000000000000"));     /* hợp lệ nhưng không tồn tại */
}

static void test_wrong_password(void) {
    session_t s; char tok[SESSION_TOKEN_LEN + 1] = "";
    session_init(&s);
    ASSERT_EQ_INT(PROTO_BAD_PASSWORD, session_connect(&s, APP_A, 0, 0, tok));
    ASSERT_EQ_INT(0, session_count(&s));
    ASSERT_STR_EQ("", tok);
}

static void test_bad_arguments(void) {
    session_t s; char tok[SESSION_TOKEN_LEN + 1];
    session_init(&s);
    ASSERT_EQ_INT(PROTO_BAD_ARG, session_connect(NULL, APP_A, 1, 0, tok));
    ASSERT_EQ_INT(PROTO_BAD_ARG, session_connect(&s, NULL, 1, 0, tok));
    ASSERT_EQ_INT(PROTO_BAD_ARG, session_connect(&s, APP_A, 1, 0, NULL));
    ASSERT_EQ_INT(PROTO_BAD_ARG, session_connect(&s, "co khoang trang", 1, 0, tok));
    ASSERT_EQ_INT(PROTO_BAD_ARG, session_connect(&s, "", 1, 0, tok));
    ASSERT_EQ_INT(PROTO_BAD_ARG, session_connect(&s, "aaaaaaaaaaaaaaaaa", 1, 0, tok)); /* 17 ký tự */
    ASSERT_EQ_INT(0, session_count(&s));
    session_init(NULL); /* không được crash */
    ASSERT_EQ_INT(0, session_count(NULL));
}

static void test_too_many_apps(void) {
    session_t s; char tok[SESSION_TOKEN_LEN + 1], id[SESSION_ID_MAX + 1];
    session_init(&s);
    for (int i = 0; i < SESSION_MAX_APPS; i++) {
        snprintf(id, sizeof id, "app%d", i);
        ASSERT_EQ_INT(PROTO_OK, session_connect(&s, id, 1, 0, tok));
    }
    ASSERT_EQ_INT(SESSION_MAX_APPS, session_count(&s));
    ASSERT_EQ_INT(PROTO_TOO_MANY_APPS, session_connect(&s, "newapp", 1, 0, tok));
    /* ứng dụng đã có vẫn kết nối lại được */
    ASSERT_EQ_INT(PROTO_OK, session_connect(&s, "app3", 1, 0, tok));
    ASSERT_EQ_INT(SESSION_MAX_APPS, session_count(&s));
}

static void test_lock_after_five_failures(void) {
    session_t s; char tok[SESSION_TOKEN_LEN + 1];
    session_init(&s);
    for (int i = 0; i < SESSION_MAX_FAILS; i++) {
        ASSERT_EQ_INT(PROTO_BAD_PASSWORD, session_connect(&s, APP_A, 0, 10 + i, tok));
    }
    ASSERT_TRUE(session_is_locked(&s, APP_A, 14));
    /* đang khóa: kể cả nhập đúng cũng bị từ chối */
    ASSERT_EQ_INT(PROTO_LOCKED, session_connect(&s, APP_A, 1, 20, tok));
    ASSERT_EQ_INT(PROTO_LOCKED, session_connect(&s, APP_A, 0, 20, tok));
    ASSERT_EQ_INT(0, session_count(&s));
    /* app khác không bị ảnh hưởng */
    ASSERT_EQ_INT(PROTO_OK, session_connect(&s, APP_B, 1, 20, tok));
    ASSERT_FALSE(session_is_locked(&s, APP_B, 20));
}

static void test_lock_expires_after_30_seconds(void) {
    session_t s; char tok[SESSION_TOKEN_LEN + 1];
    session_init(&s);
    for (int i = 0; i < SESSION_MAX_FAILS; i++) {
        session_connect(&s, APP_A, 0, 100, tok);
    }
    /* khóa tới 100 + 30 = 130 */
    ASSERT_EQ_INT(PROTO_LOCKED, session_connect(&s, APP_A, 1, 129, tok));
    ASSERT_TRUE(session_is_locked(&s, APP_A, 129));
    ASSERT_FALSE(session_is_locked(&s, APP_A, 130));
    ASSERT_EQ_INT(PROTO_OK, session_connect(&s, APP_A, 1, 130, tok));
}

static void test_lock_expired_then_fail_counter_restarts(void) {
    session_t s; char tok[SESSION_TOKEN_LEN + 1];
    session_init(&s);
    for (int i = 0; i < SESSION_MAX_FAILS; i++) {
        session_connect(&s, APP_A, 0, 0, tok);
    }
    /* sau khi hết khóa, lại phải sai đủ 5 lần mới bị khóa tiếp */
    for (int i = 0; i < SESSION_MAX_FAILS - 1; i++) {
        ASSERT_EQ_INT(PROTO_BAD_PASSWORD, session_connect(&s, APP_A, 0, 100, tok));
    }
    ASSERT_FALSE(session_is_locked(&s, APP_A, 100));
    ASSERT_EQ_INT(PROTO_BAD_PASSWORD, session_connect(&s, APP_A, 0, 100, tok));
    ASSERT_TRUE(session_is_locked(&s, APP_A, 100));
}

static void test_success_resets_fail_counter(void) {
    session_t s; char tok[SESSION_TOKEN_LEN + 1];
    session_init(&s);
    for (int i = 0; i < SESSION_MAX_FAILS - 1; i++) {
        session_connect(&s, APP_A, 0, 0, tok);
    }
    ASSERT_EQ_INT(PROTO_OK, session_connect(&s, APP_A, 1, 1, tok));
    /* bộ đếm đã xóa: thêm 4 lần sai nữa vẫn chưa bị khóa */
    for (int i = 0; i < SESSION_MAX_FAILS - 1; i++) {
        ASSERT_EQ_INT(PROTO_BAD_PASSWORD, session_connect(&s, APP_A, 0, 2, tok));
    }
    ASSERT_FALSE(session_is_locked(&s, APP_A, 2));
}

static void test_is_locked_edge_cases(void) {
    session_t s;
    session_init(&s);
    ASSERT_FALSE(session_is_locked(NULL, APP_A, 0));
    ASSERT_FALSE(session_is_locked(&s, NULL, 0));
    ASSERT_FALSE(session_is_locked(&s, APP_A, 0)); /* chưa từng sai */
}

static void test_fail_table_evicts_oldest(void) {
    session_t s; char tok[SESSION_TOKEN_LEN + 1], id[SESSION_ID_MAX + 1];
    session_init(&s);
    /* tràn bảng theo dõi: vẫn hoạt động, không crash */
    for (int i = 0; i < SESSION_MAX_TRACK + 8; i++) {
        snprintf(id, sizeof id, "x%d", i);
        ASSERT_EQ_INT(PROTO_BAD_PASSWORD, session_connect(&s, id, 0, i, tok));
    }
    /* app mới nhất vẫn bị đếm lỗi liên tục và khóa đúng */
    snprintf(id, sizeof id, "x%d", SESSION_MAX_TRACK + 7);
    for (int i = 0; i < SESSION_MAX_FAILS - 1; i++) {
        session_connect(&s, id, 0, 1000, tok);
    }
    ASSERT_TRUE(session_is_locked(&s, id, 1000));
}

static void test_revoke_others_keeps_one(void) {
    session_t s;
    char ta[SESSION_TOKEN_LEN + 1], tb[SESSION_TOKEN_LEN + 1], tc[SESSION_TOKEN_LEN + 1];
    session_init(&s);
    session_connect(&s, APP_A, 1, 0, ta);
    session_connect(&s, APP_B, 1, 0, tb);
    session_connect(&s, "cccccccccccccccc", 1, 0, tc);
    ASSERT_EQ_INT(2, session_revoke_others(&s, tb));
    ASSERT_EQ_INT(1, session_count(&s));
    ASSERT_NULL(session_validate(&s, ta));
    ASSERT_NULL(session_validate(&s, tc));
    ASSERT_STR_EQ(APP_B, session_validate(&s, tb));
}

static void test_revoke_all_and_reconnect_gets_new_token(void) {
    session_t s; char t1[SESSION_TOKEN_LEN + 1], t2[SESSION_TOKEN_LEN + 1];
    session_init(&s);
    session_connect(&s, APP_A, 1, 0, t1);
    ASSERT_EQ_INT(1, session_revoke_others(&s, NULL));
    ASSERT_EQ_INT(0, session_count(&s));
    ASSERT_NULL(session_validate(&s, t1));
    ASSERT_EQ_INT(0, session_revoke_others(&s, NULL));
    ASSERT_EQ_INT(0, session_revoke_others(NULL, NULL));
    ASSERT_EQ_INT(PROTO_OK, session_connect(&s, APP_A, 1, 0, t2));
    ASSERT_TRUE(strcmp(t1, t2) != 0); /* xác suất trùng 16^-16, bỏ qua */
}

int main(void) {
    printf("test_session\n");
    RUN_TEST(test_connect_ok_returns_valid_token);
    RUN_TEST(test_reconnect_returns_same_token);
    RUN_TEST(test_two_apps_get_different_tokens);
    RUN_TEST(test_validate_rejects_bad_tokens);
    RUN_TEST(test_wrong_password);
    RUN_TEST(test_bad_arguments);
    RUN_TEST(test_too_many_apps);
    RUN_TEST(test_lock_after_five_failures);
    RUN_TEST(test_lock_expires_after_30_seconds);
    RUN_TEST(test_lock_expired_then_fail_counter_restarts);
    RUN_TEST(test_success_resets_fail_counter);
    RUN_TEST(test_is_locked_edge_cases);
    RUN_TEST(test_fail_table_evicts_oldest);
    RUN_TEST(test_revoke_others_keeps_one);
    RUN_TEST(test_revoke_all_and_reconnect_gets_new_token);
    return MT_SUMMARY();
}
