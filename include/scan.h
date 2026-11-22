/*
 * scan.h - quét thiết bị qua UDP broadcast (DESIGN.md mục 3.3, 5.1).
 *
 * Quy trình:
 *   1. Mở socket UDP có bật SO_BROADCAST (net_udp_open_broadcast).
 *   2. Gửi một gói "SCAN <app_id>\n" tới target_ip:scan_port
 *      (mặc định: 255.255.255.255:9999).
 *   3. Thu gom các gói phản hồi "DEVICE <id> <type> <tcp_port>\n" trong
 *      khoảng thời gian timeout_ms (mặc định: 2000 ms), lấy địa chỉ IP nguồn
 *      từ recvfrom, bỏ qua gói sai định dạng và loại bỏ thiết bị trùng device_id.
 */
#ifndef SCAN_H
#define SCAN_H

#include <stddef.h>

#include "proto.h"

#define SCAN_DEFAULT_IP         "255.255.255.255"
#define SCAN_DEFAULT_PORT       9999
#define SCAN_DEFAULT_TIMEOUT_MS 2000
#define SCAN_ID_MAX             16
#define SCAN_HOST_MAX           64

typedef struct {
    char       device_id[SCAN_ID_MAX + 1];
    dev_type_t type;
    char       host[SCAN_HOST_MAX + 1];
    int        tcp_port;
} scan_device_t;

/*
 * Mở socket UDP broadcast và gửi thông điệp "SCAN <app_id>\n" tới target_ip:scan_port.
 *   target_ip : địa chỉ đích (NULL hoặc "" sẽ dùng SCAN_DEFAULT_IP "255.255.255.255").
 *   scan_port : cổng UDP đích (<= 0 sẽ dùng SCAN_DEFAULT_PORT 9999).
 *   app_id    : ID của controller (1..16 ký tự hợp lệ).
 * Trả về fd >= 0 để truyền tiếp cho scan_collect (người gọi đóng bằng net_close),
 * hoặc -1 nếu lỗi.
 */
int scan_send(const char *target_ip, int scan_port, const char *app_id);

/*
 * Thu gom các phản hồi DEVICE trên socket udp_fd trong tổng thời gian timeout_ms.
 *   timeout_ms  : thời gian chờ (ms); nếu < 0 sẽ dùng SCAN_DEFAULT_TIMEOUT_MS (2000 ms).
 *   results     : mảng nhận danh sách thiết bị quét được (đã khử trùng lặp device_id).
 *   max_results : số phần tử tối đa của mảng results (> 0).
 * Trả về số thiết bị tìm thấy (0..max_results), hoặc -1 nếu tham số sai.
 */
int scan_collect(int udp_fd, int timeout_ms, scan_device_t *results, int max_results);

/*
 * Hàm tiện ích trọn gói: gọi scan_send -> scan_collect -> net_close.
 * Trả về số thiết bị tìm thấy (>= 0), hoặc -1 nếu lỗi.
 */
int scan_devices(const char *target_ip, int scan_port, const char *app_id,
                 int timeout_ms, scan_device_t *results, int max_results);

#endif /* SCAN_H */
