# Hệ thống mô phỏng chăm sóc vườn cây tự động

Đồ án cuối kỳ môn **Thực hành Lập trình mạng (IT4062)**, viết bằng **C** (C11, POSIX) với Socket API, chạy trên Linux/WSL.

Ứng dụng gồm hai chương trình giao tiếp qua socket:

- **`device_sim`**: mô phỏng một thiết bị (cảm biến môi trường, bộ tưới nước, bộ bón phân NPK, đèn quang hợp). Mỗi thiết bị là một process có ID riêng.
- **`controller`**: ứng dụng điều khiển cho người dùng. Quản lý vườn, quét thiết bị, kết nối, đặt thông số, điều khiển.

Đề bài chi tiết: [De_bai.md](De_bai.md). Thiết kế và giao thức: [DESIGN.md](DESIGN.md).

## Thành viên

| Họ tên | MSSV | Phụ trách |
|---|---|---|
| Đinh Hữu Nhật Minh | 20235778 | Phía thiết bị (`device_sim`), các module nền tảng trong `common` |
| Lại Việt Cường | _(điền)_ | Phía controller (`appid`, `garden`, `scan`, `client`, menu CLI), `proto`, `netutil` |

## Yêu cầu môi trường

- Ubuntu (WSL2 trên Windows hoặc Linux thật)
- `gcc`, `make`, `gdb`, `gcovr` (đo coverage)

```bash
sudo apt update
sudo apt install build-essential gdb gcovr
```

## Build và chạy

> Chạy trong WSL/Ubuntu. `make` và `make test` dùng được ngay. `device_sim` đã build được; `controller` sẽ build được khi có `main.c` tương ứng.

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

Có thể thử giao thức thủ công bằng `nc 127.0.0.1 7002`, ví dụ:

```
CONNECT 9f3a1c0b7d2e4a58 1234
<token nhận được> INFO
```

Tham số của `device_sim`: `--type --id --port` (bắt buộc), `--password` (bắt buộc ở lần chạy đầu), `--timescale`, `--state-dir`, `--log-dir`, `--bind`, `--scan-port`. Chạy `./build/device_sim --help` để xem chi tiết. Mật khẩu đã đổi được lưu trong `data/<ID>.state`, log ghi ở `logs/<ID>.log` (giờ UTC).

## Cấu trúc thư mục

```
include/   header của các module
src/common/      linebuf, proto, netutil, simclock, log, util
src/device/      main, options, server poll(), device, session, store, dev_sensor/pump/fert/light
src/controller/  menu CLI, garden, scan, client, appid
tests/unit/ tests/integration/ tests/load/
docs/      báo cáo, slide, test case
```

## Tiến độ

- [x] Thiết kế kiến trúc và giao thức
- [x] Khung dự án, Makefile, framework test
- [x] Module `common` (linebuf, proto, util, simclock, log, netutil)
- [ ] Phía thiết bị (đã xong: `session`, `store`, `device` + `server` poll với các lệnh chung; còn `dev_sensor`, `dev_pump`, `dev_fert`, `dev_light`)
- [ ] Phía controller
- [ ] Integration test, test chịu tải
- [ ] Báo cáo, slide
