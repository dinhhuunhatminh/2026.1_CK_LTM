/* Unit test cho appid: sinh mới, lưu file, đọc lại và xử lý file hỏng. */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define RMDIR(p) _rmdir(p)
#else
#include <unistd.h>
#define RMDIR(p) rmdir(p)
#endif

#include "appid.h"
#include "minitest.h"
#include "util.h"

static void cleanup_dir(const char *dir) {
    char path[256];
    snprintf(path, sizeof path, "%s/%s", dir, APPID_FILENAME);
    remove(path);
    RMDIR(path);
    RMDIR(dir);
}

static void write_raw_file(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    if (fp != NULL) {
        fputs(content, fp);
        fclose(fp);
    }
}

static void test_generate_and_persist(void) {
    const char *base = "test_appid_dir.tmp";
    const char *sub  = "test_appid_dir.tmp/controller";
    char id1[APPID_BUF_SIZE];
    char id2[APPID_BUF_SIZE];

    cleanup_dir(sub);
    RMDIR(base);

    /* Lần đầu chưa có thư mục/file -> tự tạo thư mục cha+con và sinh ID 16 hex */
    ASSERT_EQ_INT(0, appid_get(sub, id1, sizeof id1));
    ASSERT_EQ_INT(1, util_valid_hex(id1, APPID_LEN));

    /* Lần hai gọi lại trên cùng thư mục -> đọc lại đúng ID cũ */
    ASSERT_EQ_INT(0, appid_get(sub, id2, sizeof id2));
    ASSERT_STR_EQ(id1, id2);

    cleanup_dir(sub);
    RMDIR(base);
}

static void test_corrupted_or_empty_file_regenerates(void) {
    const char *dir = "test_appid_bad.tmp";
    char path[256];
    char id1[APPID_BUF_SIZE];
    char id2[APPID_BUF_SIZE];

    cleanup_dir(dir);
    snprintf(path, sizeof path, "%s/%s", dir, APPID_FILENAME);

    /* Tạo thư mục và ghi file rỗng -> phải sinh mới và ghi đè */
    ASSERT_EQ_INT(0, appid_get(dir, id1, sizeof id1));
    write_raw_file(path, "");
    ASSERT_EQ_INT(0, appid_get(dir, id2, sizeof id2));
    ASSERT_EQ_INT(1, util_valid_hex(id2, APPID_LEN));

    /* File chứa ký tự không phải hex hoặc sai độ dài -> sinh mới và ghi đè */
    write_raw_file(path, "not_a_valid_hex!\n");
    ASSERT_EQ_INT(0, appid_get(dir, id1, sizeof id1));
    ASSERT_EQ_INT(1, util_valid_hex(id1, APPID_LEN));

    write_raw_file(path, "1234abcd\n");
    ASSERT_EQ_INT(0, appid_get(dir, id2, sizeof id2));
    ASSERT_EQ_INT(1, util_valid_hex(id2, APPID_LEN));

    cleanup_dir(dir);
}

static void test_invalid_args_and_unwritable_dir(void) {
    const char *blocker_file = "test_appid_blocker.tmp";
    char id[APPID_BUF_SIZE];
    char long_dir[600];

    ASSERT_EQ_INT(-1, appid_get(NULL, id, sizeof id));
    ASSERT_EQ_INT(-1, appid_get("", id, sizeof id));
    ASSERT_EQ_INT(-1, appid_get("dir", NULL, sizeof id));
    ASSERT_EQ_INT(-1, appid_get("dir", id, APPID_LEN)); /* bộ đệm thiếu chỗ cho '\0' */

    memset(long_dir, 'a', sizeof(long_dir) - 1);
    long_dir[sizeof(long_dir) - 1] = '\0';
    ASSERT_EQ_INT(-1, appid_get(long_dir, id, sizeof id));

    /* Tạo một file thường trùng tên thư mục -> không thể tạo/ghi app_id.txt bên trong */
    write_raw_file(blocker_file, "x");
    ASSERT_EQ_INT(-1, appid_get(blocker_file, id, sizeof id));
    remove(blocker_file);
}

int main(void) {
    printf("test_appid\n");
    RUN_TEST(test_generate_and_persist);
    RUN_TEST(test_corrupted_or_empty_file_regenerates);
    RUN_TEST(test_invalid_args_and_unwritable_dir);
    return MT_SUMMARY();
}
