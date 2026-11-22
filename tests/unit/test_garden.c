/* Unit test cho garden: CRUD vườn, quản lý thiết bị trong vườn, lưu/đọc file và xử lý file hỏng. */
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define RMDIR(p) _rmdir(p)
#else
#include <unistd.h>
#define RMDIR(p) rmdir(p)
#endif

#include "garden.h"
#include "minitest.h"

static void cleanup_garden_dir(const char *dir) {
    char path[256];
    snprintf(path, sizeof path, "%s/%s", dir, GARDEN_FILENAME);
    remove(path);
    snprintf(path, sizeof path, "%s/%s.tmp", dir, GARDEN_FILENAME);
    remove(path);
    RMDIR(dir);
}

static void write_raw(const char *dir, const char *content) {
    char path[256];
    FILE *fp;
    snprintf(path, sizeof path, "%s/%s", dir, GARDEN_FILENAME);
    fp = fopen(path, "w");
    if (fp != NULL) {
        fputs(content, fp);
        fclose(fp);
    }
}

static void test_garden_crud_and_formatting(void) {
    garden_list_t list;
    int id1 = 0, id2 = 0;
    char buf[1024];
    const garden_t *cg;

    garden_list_init(NULL);
    garden_list_init(&list);
    ASSERT_EQ_INT(0, list.count);

    /* Danh sách rỗng */
    ASSERT_TRUE(garden_format_list(&list, buf, sizeof buf) > 0);
    ASSERT_TRUE(strstr(buf, "chưa có vườn") != NULL);

    /* Thêm 2 vườn có khoảng trắng trong tên, cây trồng, ghi chú */
    ASSERT_EQ_INT(0, garden_add(&list, "Vườn Rau Sân Thượng", "Cải ngọt", 25.5, "Tưới sáng", &id1));
    ASSERT_EQ_INT(1, id1);
    ASSERT_EQ_INT(0, garden_add(&list, "Vườn Hoa Hồng", "Hoa hồng Đà Lạt", 40.0, NULL, &id2));
    ASSERT_EQ_INT(2, id2);
    ASSERT_EQ_INT(2, list.count);

    /* Sửa thông tin vườn 1 */
    ASSERT_EQ_INT(0, garden_update(&list, id1, "Vườn Rau Hữu Cơ", "Xà lách", 30.0, "Ghi chú mới"));
    cg = garden_find_const(&list, id1);
    ASSERT_NOT_NULL(cg);
    ASSERT_STR_EQ("Vườn Rau Hữu Cơ", cg->name);
    ASSERT_STR_EQ("Xà lách", cg->crop);
    ASSERT_NEAR(30.0, cg->area, 1e-6);
    ASSERT_STR_EQ("Ghi chú mới", cg->note);

    ASSERT_TRUE(garden_format_list(&list, buf, sizeof buf) > 0);
    ASSERT_TRUE(strstr(buf, "Vườn Rau Hữu Cơ") != NULL);
    ASSERT_TRUE(garden_format_detail(cg, buf, sizeof buf) > 0);
    ASSERT_TRUE(strstr(buf, "Xà lách") != NULL);

    /* Xóa vườn 1, còn lại vườn 2 */
    ASSERT_EQ_INT(0, garden_remove(&list, id1));
    ASSERT_EQ_INT(1, list.count);
    ASSERT_NULL(garden_find(&list, id1));
    ASSERT_NULL(garden_find_const(&list, id1));
    ASSERT_NOT_NULL(garden_find(&list, id2));

    /* Lỗi tham số khi thêm/sửa/xóa */
    ASSERT_EQ_INT(-1, garden_add(NULL, "A", "B", 10, "", NULL));
    ASSERT_EQ_INT(-1, garden_add(&list, "", "B", 10, "", NULL));
    ASSERT_EQ_INT(-1, garden_add(&list, "A", "", 10, "", NULL));
    ASSERT_EQ_INT(-1, garden_add(&list, "A", "B", 0, "", NULL));
    ASSERT_EQ_INT(-1, garden_add(&list, "A", "B", -5, "", NULL));
    ASSERT_EQ_INT(-1, garden_add(&list, "A\nB", "B", 10, "", NULL));
    ASSERT_EQ_INT(-1, garden_update(&list, 999, "A", "B", 10, ""));
    ASSERT_EQ_INT(-1, garden_remove(&list, 999));
    ASSERT_EQ_INT(-1, garden_remove(NULL, 1));
    ASSERT_EQ_INT(-1, garden_format_list(NULL, buf, sizeof buf));
    ASSERT_EQ_INT(-1, garden_format_list(&list, buf, 5));
    ASSERT_EQ_INT(-1, garden_format_detail(NULL, buf, sizeof buf));
    ASSERT_EQ_INT(-1, garden_format_detail(cg, buf, 5));
}

static void test_devices_in_garden_and_limits(void) {
    garden_list_t list;
    garden_t *g;
    int gid = 0;
    char id[16];
    char buf[2048];

    garden_list_init(&list);
    ASSERT_EQ_INT(0, garden_add(&list, "Vườn 1", "Cà chua", 50.0, "", &gid));
    g = garden_find(&list, gid);
    ASSERT_NOT_NULL(g);

    ASSERT_EQ_INT(0, garden_add_device(g, "S01", DEV_SENSOR, "127.0.0.1", 7001, NULL));
    ASSERT_EQ_INT(0, garden_add_device(g, "P01", DEV_PUMP, "127.0.0.1", 7002, "5b1e77a09c3d4f21"));
    ASSERT_EQ_INT(2, g->device_count);

    /* Không cho phép trùng device_id trong cùng vườn */
    ASSERT_EQ_INT(-1, garden_add_device(g, "S01", DEV_SENSOR, "127.0.0.1", 7005, NULL));

    /* Cập nhật token của S01 */
    ASSERT_EQ_INT(0, garden_set_device_token(g, "S01", "9f3a1c0b7d2e4a58"));
    ASSERT_STR_EQ("9f3a1c0b7d2e4a58", garden_find_device(g, "S01")->token);
    ASSERT_EQ_INT(0, garden_set_device_token(g, "S01", NULL));
    ASSERT_STR_EQ("", garden_find_device(g, "S01")->token);
    ASSERT_EQ_INT(-1, garden_set_device_token(g, "NO_SUCH", "abc"));
    ASSERT_EQ_INT(-1, garden_set_device_token(g, "S01", "bad token space"));

    ASSERT_TRUE(garden_format_detail(g, buf, sizeof buf) > 0);
    ASSERT_TRUE(strstr(buf, "S01 (SENSOR)") != NULL);

    /* Xóa thiết bị S01 */
    ASSERT_EQ_INT(0, garden_remove_device(g, "S01"));
    ASSERT_EQ_INT(1, g->device_count);
    ASSERT_EQ_INT(-1, garden_remove_device(g, "S01"));
    ASSERT_EQ_INT(-1, garden_remove_device(NULL, "P01"));

    /* Thêm tới giới hạn 32 thiết bị, cái thứ 33 phải bị từ chối */
    for (int i = 2; i <= GARDEN_MAX_DEVICES; i++) {
        snprintf(id, sizeof id, "D%02d", i);
        ASSERT_EQ_INT(0, garden_add_device(g, id, DEV_LIGHT, "127.0.0.1", 7000 + i, ""));
    }
    ASSERT_EQ_INT(GARDEN_MAX_DEVICES, g->device_count);
    ASSERT_EQ_INT(-1, garden_add_device(g, "EXTRA", DEV_LIGHT, "127.0.0.1", 8000, ""));

    /* Kiểm tra tham số thiết bị sai */
    ASSERT_EQ_INT(-1, garden_add_device(NULL, "X1", DEV_PUMP, "127.0.0.1", 7000, ""));
    ASSERT_EQ_INT(0, garden_remove_device(g, "D02"));
    ASSERT_EQ_INT(-1, garden_add_device(g, "bad id", DEV_PUMP, "127.0.0.1", 7000, ""));
    ASSERT_EQ_INT(-1, garden_add_device(g, "X1", DEV_NONE, "127.0.0.1", 7000, ""));
    ASSERT_EQ_INT(-1, garden_add_device(g, "X1", DEV_PUMP, "bad|host", 7000, ""));
    ASSERT_EQ_INT(-1, garden_add_device(g, "X1", DEV_PUMP, "127.0.0.1", 0, ""));
}

static void test_save_and_load_roundtrip(void) {
    const char *dir = "test_garden_store.tmp";
    garden_list_t src, dst;
    int id1 = 0, id2 = 0;
    garden_t *g1, *g2;

    cleanup_garden_dir(dir);

    /* Chưa có file -> trả về GARDEN_NOT_FOUND và khởi tạo rỗng */
    ASSERT_EQ_INT(GARDEN_NOT_FOUND, garden_load(&dst, dir));
    ASSERT_EQ_INT(0, dst.count);

    garden_list_init(&src);
    ASSERT_EQ_INT(0, garden_add(&src, "Vườn Cây Ăn Quả", "Cam sành", 120.5, "Phía đông", &id1));
    ASSERT_EQ_INT(0, garden_add(&src, "Vườn Ươm", "Dâu tây", 15.25, "", &id2));
    g1 = garden_find(&src, id1);
    g2 = garden_find(&src, id2);
    ASSERT_EQ_INT(0, garden_add_device(g1, "S01", DEV_SENSOR, "127.0.0.1", 7001, ""));
    ASSERT_EQ_INT(0, garden_add_device(g1, "P01", DEV_PUMP, "127.0.0.1", 7002, "5b1e77a09c3d4f21"));
    ASSERT_EQ_INT(0, garden_add_device(g2, "F01", DEV_FERT, "192.168.1.10", 7003, "abcdef0123456789"));

    ASSERT_EQ_INT(0, garden_save(&src, dir));
    ASSERT_EQ_INT(GARDEN_OK, garden_load(&dst, dir));

    ASSERT_EQ_INT(src.count, dst.count);
    for (int i = 0; i < src.count; i++) {
        ASSERT_EQ_INT(src.items[i].id, dst.items[i].id);
        ASSERT_STR_EQ(src.items[i].name, dst.items[i].name);
        ASSERT_STR_EQ(src.items[i].crop, dst.items[i].crop);
        ASSERT_NEAR(src.items[i].area, dst.items[i].area, 1e-4);
        ASSERT_STR_EQ(src.items[i].note, dst.items[i].note);
        ASSERT_EQ_INT(src.items[i].device_count, dst.items[i].device_count);
        for (int j = 0; j < src.items[i].device_count; j++) {
            ASSERT_STR_EQ(src.items[i].devices[j].device_id, dst.items[i].devices[j].device_id);
            ASSERT_EQ_INT(src.items[i].devices[j].type, dst.items[i].devices[j].type);
            ASSERT_STR_EQ(src.items[i].devices[j].host, dst.items[i].devices[j].host);
            ASSERT_EQ_INT(src.items[i].devices[j].port, dst.items[i].devices[j].port);
            ASSERT_STR_EQ(src.items[i].devices[j].token, dst.items[i].devices[j].token);
        }
    }

    cleanup_garden_dir(dir);
}

static void test_corrupted_files_rejected(void) {
    const char *dir = "test_garden_bad.tmp";
    garden_list_t list;
    int gid = 0;

    cleanup_garden_dir(dir);
    garden_list_init(&list);
    garden_add(&list, "Vườn Gốc", "Lúa", 10.0, "", &gid);
    ASSERT_EQ_INT(0, garden_save(&list, dir));

    /* Dòng nằm ngoài block [GARDEN] */
    write_raw(dir, "id=1\n");
    ASSERT_EQ_INT(GARDEN_ERR, garden_load(&list, dir));
    ASSERT_EQ_INT(1, list.count); /* list cũ được giữ nguyên khi lỗi */

    /* Block chưa đóng [/GARDEN] */
    write_raw(dir, "[GARDEN]\nid=1\nname=A\ncrop=B\narea=10\n");
    ASSERT_EQ_INT(GARDEN_ERR, garden_load(&list, dir));

    /* Thiếu trường bắt buộc (thiếu crop) */
    write_raw(dir, "[GARDEN]\nid=1\nname=A\narea=10\n[/GARDEN]\n");
    ASSERT_EQ_INT(GARDEN_ERR, garden_load(&list, dir));

    /* Khoá lạ hoặc dòng device sai định dạng */
    write_raw(dir, "[GARDEN]\nid=1\nname=A\ncrop=B\narea=10\nunknown=1\n[/GARDEN]\n");
    ASSERT_EQ_INT(GARDEN_ERR, garden_load(&list, dir));

    write_raw(dir, "[GARDEN]\nid=1\nname=A\ncrop=B\narea=10\ndevice=S01|SENSOR|127.0.0.1\n[/GARDEN]\n");
    ASSERT_EQ_INT(GARDEN_ERR, garden_load(&list, dir));

    write_raw(dir, "[GARDEN]\nid=1\nname=A\ncrop=B\narea=10\ndevice=S01|SENSOR|127.0.0.1|badport|\n[/GARDEN]\n");
    ASSERT_EQ_INT(GARDEN_ERR, garden_load(&list, dir));

    ASSERT_EQ_INT(-1, garden_save(NULL, dir));
    ASSERT_EQ_INT(-1, garden_save(&list, ""));
    ASSERT_EQ_INT(GARDEN_ERR, garden_load(NULL, dir));
    ASSERT_EQ_INT(GARDEN_ERR, garden_load(&list, ""));

    cleanup_garden_dir(dir);
}

int main(void) {
    printf("test_garden\n");
    RUN_TEST(test_garden_crud_and_formatting);
    RUN_TEST(test_devices_in_garden_and_limits);
    RUN_TEST(test_save_and_load_roundtrip);
    RUN_TEST(test_corrupted_files_rejected);
    return MT_SUMMARY();
}
