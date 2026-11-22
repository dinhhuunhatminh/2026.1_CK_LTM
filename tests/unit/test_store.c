/* Unit test cho store: lưu/đọc mật khẩu và tham số, xử lý file hỏng. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "minitest.h"
#include "store.h"

static char g_dir[128];

/* Tạo thư mục tạm riêng cho mỗi test. */
static void new_dir(void) {
    snprintf(g_dir, sizeof g_dir, "/tmp/store_test_XXXXXX");
    if (mkdtemp(g_dir) == NULL) {
        g_dir[0] = '\0';
    }
}

static void drop_dir(void) {
    char cmd[200];
    if (g_dir[0]) {
        snprintf(cmd, sizeof cmd, "rm -rf '%s'", g_dir);
        if (system(cmd) != 0) {
            /* bỏ qua: chỉ là dọn dẹp */
        }
    }
}

/* Ghi nội dung thô vào <g_dir>/<id>.state. */
static void write_raw(const char *id, const char *content) {
    char path[256];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s.state", g_dir, id);
    f = fopen(path, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

static void test_set_get_and_override(void) {
    store_t s;
    store_init(&s);
    ASSERT_NULL(store_get(&s, "HMIN"));
    ASSERT_EQ_INT(0, store_set(&s, "HMIN", "30"));
    ASSERT_STR_EQ("30", store_get(&s, "HMIN"));
    ASSERT_EQ_INT(0, store_set(&s, "HMIN", "35"));
    ASSERT_STR_EQ("35", store_get(&s, "HMIN"));
    ASSERT_EQ_INT(1, s.count);
    ASSERT_EQ_INT(0, store_set(&s, "SCHEDULE", "06:00,17:00"));
    ASSERT_EQ_INT(0, store_set(&s, "NOTE", "co dau cach o giua"));
    ASSERT_STR_EQ("co dau cach o giua", store_get(&s, "NOTE"));
    ASSERT_EQ_INT(0, store_set(&s, "EMPTY", ""));
    ASSERT_STR_EQ("", store_get(&s, "EMPTY"));
}

static void test_set_rejects_invalid(void) {
    store_t s;
    char big[STORE_VAL_MAX + 8];
    store_init(&s);
    ASSERT_EQ_INT(-1, store_set(NULL, "A", "1"));
    ASSERT_EQ_INT(-1, store_set(&s, NULL, "1"));
    ASSERT_EQ_INT(-1, store_set(&s, "A", NULL));
    ASSERT_EQ_INT(-1, store_set(&s, "", "1"));
    ASSERT_EQ_INT(-1, store_set(&s, "co dau cach", "1"));
    ASSERT_EQ_INT(-1, store_set(&s, "a=b", "1"));
    ASSERT_EQ_INT(-1, store_set(&s, "password", "abcd")); /* khóa dành riêng */
    ASSERT_EQ_INT(-1, store_set(&s, "A", "xuong\ndong"));
    ASSERT_EQ_INT(-1, store_set(&s, "A", "tab\there"));
    ASSERT_EQ_INT(-1, store_set(&s, "A", "\x7f"));
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    ASSERT_EQ_INT(-1, store_set(&s, "A", big));
    memset(big, 'k', STORE_KEY_MAX + 1);
    big[STORE_KEY_MAX + 1] = '\0';
    ASSERT_EQ_INT(-1, store_set(&s, big, "1"));
    ASSERT_EQ_INT(0, s.count);
    ASSERT_NULL(store_get(NULL, "A"));
    ASSERT_NULL(store_get(&s, NULL));
}

static void test_table_full(void) {
    store_t s;
    char key[16];
    store_init(&s);
    for (int i = 0; i < STORE_MAX_KV; i++) {
        snprintf(key, sizeof key, "K%d", i);
        ASSERT_EQ_INT(0, store_set(&s, key, "v"));
    }
    ASSERT_EQ_INT(-1, store_set(&s, "TOOMANY", "v"));
    ASSERT_EQ_INT(0, store_set(&s, "K0", "moi")); /* ghi đè khóa cũ vẫn được */
    ASSERT_STR_EQ("moi", store_get(&s, "K0"));
}

static void test_password_rules(void) {
    store_t s;
    char longpw[STORE_PASS_MAX + 4];
    store_init(&s);
    ASSERT_FALSE(store_password_matches(&s, "1234")); /* chưa đặt mật khẩu */
    ASSERT_EQ_INT(-1, store_set_password(NULL, "1234"));
    ASSERT_EQ_INT(-1, store_set_password(&s, NULL));
    ASSERT_EQ_INT(-1, store_set_password(&s, "abc"));          /* quá ngắn */
    ASSERT_EQ_INT(-1, store_set_password(&s, "co khoang"));    /* có dấu cách */
    ASSERT_EQ_INT(-1, store_set_password(&s, "ab\ncd"));       /* xuống dòng */
    memset(longpw, 'p', sizeof longpw - 1);
    longpw[sizeof longpw - 1] = '\0';
    ASSERT_EQ_INT(-1, store_set_password(&s, longpw));         /* quá dài */
    ASSERT_EQ_INT(0, store_set_password(&s, "1234"));
    ASSERT_TRUE(store_password_matches(&s, "1234"));
    ASSERT_FALSE(store_password_matches(&s, "12345"));
    ASSERT_FALSE(store_password_matches(&s, NULL));
    ASSERT_FALSE(store_password_matches(NULL, "1234"));
    memset(longpw, 'p', STORE_PASS_MAX);
    longpw[STORE_PASS_MAX] = '\0';
    ASSERT_EQ_INT(0, store_set_password(&s, longpw));          /* đúng 32 ký tự */
}

static void test_save_and_load_roundtrip(void) {
    store_t a, b;
    struct stat st;
    char path[256];
    new_dir();
    store_init(&a);
    store_set_password(&a, "matkhau9");
    store_set(&a, "HMIN", "30");
    store_set(&a, "SCHEDULE", "06:00,17:00");
    store_set(&a, "SENSOR", "127.0.0.1 7001 1234");
    ASSERT_EQ_INT(0, store_save(&a, g_dir, "P01"));

    store_init(&b);
    ASSERT_EQ_INT(STORE_OK, store_load(&b, g_dir, "P01"));
    ASSERT_STR_EQ("matkhau9", b.password);
    ASSERT_EQ_INT(3, b.count);
    ASSERT_STR_EQ("30", store_get(&b, "HMIN"));
    ASSERT_STR_EQ("06:00,17:00", store_get(&b, "SCHEDULE"));
    ASSERT_STR_EQ("127.0.0.1 7001 1234", store_get(&b, "SENSOR"));

    /* file chứa mật khẩu nên chỉ chủ sở hữu được đọc/ghi; không để lại file .tmp */
    snprintf(path, sizeof path, "%s/P01.state", g_dir);
    ASSERT_EQ_INT(0, stat(path, &st));
    ASSERT_EQ_INT(0600, st.st_mode & 0777);
    snprintf(path, sizeof path, "%s/P01.state.tmp", g_dir);
    ASSERT_EQ_INT(-1, stat(path, &st));
    drop_dir();
}

static void test_save_overwrites_and_creates_dir(void) {
    store_t a, b;
    char sub[200];
    new_dir();
    snprintf(sub, sizeof sub, "%s/moi", g_dir); /* thư mục chưa tồn tại */
    store_init(&a);
    store_set_password(&a, "mk1234");
    store_set(&a, "WATT", "100");
    ASSERT_EQ_INT(0, store_save(&a, sub, "L01"));
    store_set_password(&a, "mk5678");
    store_set(&a, "WATT", "250");
    ASSERT_EQ_INT(0, store_save(&a, sub, "L01"));
    store_init(&b);
    ASSERT_EQ_INT(STORE_OK, store_load(&b, sub, "L01"));
    ASSERT_STR_EQ("mk5678", b.password);
    ASSERT_STR_EQ("250", store_get(&b, "WATT"));
    drop_dir();
}

static void test_load_not_found_keeps_store(void) {
    store_t s;
    new_dir();
    store_init(&s);
    store_set_password(&s, "macdinh");
    ASSERT_EQ_INT(STORE_NOT_FOUND, store_load(&s, g_dir, "S01"));
    ASSERT_STR_EQ("macdinh", s.password);
    drop_dir();
}

static void test_load_accepts_comments_blank_and_crlf(void) {
    store_t s;
    new_dir();
    write_raw("F01", "# ghi chu\r\n\r\n  \npassword=abcd\r\nNMIN=20\r\n# het\r\nCONC=5\r\n");
    store_init(&s);
    ASSERT_EQ_INT(STORE_OK, store_load(&s, g_dir, "F01"));
    ASSERT_STR_EQ("abcd", s.password);
    ASSERT_STR_EQ("20", store_get(&s, "NMIN"));
    ASSERT_STR_EQ("5", store_get(&s, "CONC"));
    ASSERT_EQ_INT(2, s.count);
    drop_dir();
}

static void test_load_duplicate_key_last_wins(void) {
    store_t s;
    new_dir();
    write_raw("S01", "password=abcd\nT=5\nT=10\n");
    store_init(&s);
    ASSERT_EQ_INT(STORE_OK, store_load(&s, g_dir, "S01"));
    ASSERT_STR_EQ("10", store_get(&s, "T"));
    ASSERT_EQ_INT(1, s.count);
    drop_dir();
}

static void expect_corrupt(const char *content) {
    store_t s;
    store_init(&s);
    store_set_password(&s, "giunguyen");
    store_set(&s, "A", "1");
    write_raw("X01", content);
    ASSERT_EQ_INT(STORE_ERR, store_load(&s, g_dir, "X01"));
    /* file hỏng không được làm thay đổi dữ liệu đang có */
    ASSERT_STR_EQ("giunguyen", s.password);
    ASSERT_STR_EQ("1", store_get(&s, "A"));
}

static void test_load_corrupt_files(void) {
    char longline[STORE_LINE_MAX + 40];
    new_dir();
    expect_corrupt("");                                   /* rỗng: thiếu mật khẩu */
    expect_corrupt("HMIN=30\n");                          /* thiếu mật khẩu */
    expect_corrupt("password=abc\n");                     /* mật khẩu quá ngắn */
    expect_corrupt("password=abcd\ndong khong co dau bang\n");
    expect_corrupt("password=abcd\nbad key=1\n");         /* khóa sai */
    expect_corrupt("password=abcd\n=giatri\n");           /* khóa rỗng */
    expect_corrupt("password=abcd\nCONC = 5\n");          /* khóa có dấu cách */
    expect_corrupt("password=co dau cach\n");             /* mật khẩu có dấu cách */
    memset(longline, 'a', sizeof longline - 1);
    longline[sizeof longline - 1] = '\0';
    expect_corrupt(longline);                             /* dòng quá dài */
    drop_dir();
}

static void test_load_too_many_entries_is_corrupt(void) {
    char content[2048] = "password=abcd\n";
    char one[32];
    for (int i = 0; i <= STORE_MAX_KV; i++) {
        snprintf(one, sizeof one, "K%d=v\n", i);
        strcat(content, one);
    }
    new_dir();
    expect_corrupt(content);
    drop_dir();
}

static void test_load_directory_instead_of_file(void) {
    store_t s;
    char path[256];
    new_dir();
    snprintf(path, sizeof path, "%s/D01.state", g_dir);
    ASSERT_EQ_INT(0, mkdir(path, 0700));
    store_init(&s);
    ASSERT_EQ_INT(STORE_ERR, store_load(&s, g_dir, "D01"));
    drop_dir();
}

static void test_invalid_arguments(void) {
    store_t s;
    store_init(&s);
    store_set_password(&s, "abcd");
    new_dir();
    ASSERT_EQ_INT(-1, store_save(NULL, g_dir, "P01"));
    ASSERT_EQ_INT(-1, store_save(&s, NULL, "P01"));
    ASSERT_EQ_INT(-1, store_save(&s, "", "P01"));
    ASSERT_EQ_INT(-1, store_save(&s, g_dir, NULL));
    ASSERT_EQ_INT(-1, store_save(&s, g_dir, ""));
    ASSERT_EQ_INT(-1, store_save(&s, g_dir, "../thoat"));      /* chặn path traversal */
    ASSERT_EQ_INT(-1, store_save(&s, g_dir, "a/b"));
    ASSERT_EQ_INT(-1, store_save(&s, g_dir, "id_qua_dai_17_kti")); /* 17 ký tự, vượt giới hạn 16 */
    ASSERT_EQ_INT(STORE_ERR, store_load(NULL, g_dir, "P01"));
    ASSERT_EQ_INT(STORE_ERR, store_load(&s, NULL, "P01"));
    ASSERT_EQ_INT(STORE_ERR, store_load(&s, g_dir, "../thoat"));
    ASSERT_EQ_INT(STORE_ERR, store_load(&s, "", "P01"));
    drop_dir();
}

static void test_save_requires_password(void) {
    store_t s;
    store_init(&s);
    new_dir();
    ASSERT_EQ_INT(-1, store_save(&s, g_dir, "P01"));
    drop_dir();
}

static void test_save_fails_when_dir_is_a_file(void) {
    store_t s;
    char path[256];
    FILE *f;
    store_init(&s);
    store_set_password(&s, "abcd");
    new_dir();
    snprintf(path, sizeof path, "%s/la_file", g_dir);
    f = fopen(path, "w");
    ASSERT_NOT_NULL(f);
    fclose(f);
    ASSERT_EQ_INT(-1, store_save(&s, path, "P01"));
    drop_dir();
}

static void test_save_fails_when_path_too_long(void) {
    store_t s;
    char dir[600];
    store_init(&s);
    store_set_password(&s, "abcd");
    memset(dir, 'd', sizeof dir - 1);
    dir[sizeof dir - 1] = '\0';
    ASSERT_EQ_INT(-1, store_save(&s, dir, "P01"));
    ASSERT_EQ_INT(STORE_ERR, store_load(&s, dir, "P01"));
}

int main(void) {
    printf("test_store\n");
    RUN_TEST(test_set_get_and_override);
    RUN_TEST(test_set_rejects_invalid);
    RUN_TEST(test_table_full);
    RUN_TEST(test_password_rules);
    RUN_TEST(test_save_and_load_roundtrip);
    RUN_TEST(test_save_overwrites_and_creates_dir);
    RUN_TEST(test_load_not_found_keeps_store);
    RUN_TEST(test_load_accepts_comments_blank_and_crlf);
    RUN_TEST(test_load_duplicate_key_last_wins);
    RUN_TEST(test_load_corrupt_files);
    RUN_TEST(test_load_too_many_entries_is_corrupt);
    RUN_TEST(test_load_directory_instead_of_file);
    RUN_TEST(test_invalid_arguments);
    RUN_TEST(test_save_requires_password);
    RUN_TEST(test_save_fails_when_dir_is_a_file);
    RUN_TEST(test_save_fails_when_path_too_long);
    return MT_SUMMARY();
}
