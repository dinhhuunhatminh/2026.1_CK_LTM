# Hệ thống mô phỏng chăm sóc vườn cây tự động

Đồ án cuối kỳ môn **Thực hành Lập trình mạng (IT4062)**, viết bằng **C** (C11, POSIX) với Socket API, chạy trên Linux/WSL.

Ứng dụng gồm hai chương trình giao tiếp qua socket:

- **`device_sim`**: mô phỏng một thiết bị (cảm biến môi trường, bộ tưới nước, bộ bón phân NPK, đèn quang hợp). Mỗi thiết bị là một process có ID riêng.
- **`controller`**: ứng dụng điều khiển cho người dùng. Quản lý vườn, quét thiết bị, kết nối, đặt thông số, điều khiển.

Đề bài chi tiết: [De_bai.md](De_bai.md). Thiết kế và giao thức: [DESIGN.md](DESIGN.md).

## Thành viên

| Họ tên | MSSV | Phụ trách |
|---|---|---|
| _(điền)_ | _(điền)_ | _(điền)_ |
| _(điền)_ | _(điền)_ | _(điền)_ |

## Yêu cầu môi trường

- Ubuntu (WSL2 trên Windows hoặc Linux thật)
- `gcc`, `make`, `gdb`, `gcovr` (đo coverage)

```bash
sudo apt update
sudo apt install build-essential gdb gcovr
```

## Build và chạy

> Các lệnh dưới đây sẽ dùng được sau khi có `Makefile` (commit tiếp theo).

```bash
make                # build device_sim, controller
make test           # chạy unit test + integration test
make coverage       # báo cáo coverage -> build/coverage/index.html
make loadtest       # chạy bài test chịu tải
```

Chạy thử bốn thiết bị và controller (mỗi lệnh một terminal):

```bash
./build/device_sim --type sensor --id S01 --port 7001 --password 1234
./build/device_sim --type pump   --id P01 --port 7002 --password 1234
./build/device_sim --type fert   --id F01 --port 7003 --password 1234
./build/device_sim --type light  --id L01 --port 7004 --password 1234
./build/controller
```

Có thể thử giao thức thủ công bằng `nc 127.0.0.1 7002`.

## Cấu trúc thư mục

```
include/   header của các module
src/common/      linebuf, proto, netutil, simclock, log, util
src/device/      server poll(), session, store, dev_sensor/pump/fert/light
src/controller/  menu CLI, garden, scan, client, appid
tests/unit/ tests/integration/ tests/load/
docs/      báo cáo, slide, test case
```

## Tiến độ

- [x] Thiết kế kiến trúc và giao thức
- [ ] Khung dự án, Makefile, framework test
- [ ] Module `common`
- [ ] Phía thiết bị
- [ ] Phía controller
- [ ] Integration test, test chịu tải
- [ ] Báo cáo, slide
