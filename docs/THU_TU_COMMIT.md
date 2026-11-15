# Thứ tự commit và hướng dẫn làm việc

Nhóm 2 người: **A – Đinh Hữu Nhật Minh (20235778)** và **B – Lại Việt Cường**.
Đọc trước: [README.md](../README.md), [DESIGN.md](../DESIGN.md) (đặc biệt mục 3 giao thức và mục 8 phân công).

## Quy tắc chung

1. Hai commit liên tiếp (của ai cũng vậy) cách nhau **ít nhất 30 phút**.
2. Mỗi người commit bằng **tài khoản git của chính mình** (kiểm tra `git config user.name` và `git config user.email`).
3. Luôn **`git pull` trước khi bắt đầu làm**.
4. Trước khi commit, `make test` phải đạt ("TẤT CẢ TEST ĐẠT").
5. Mỗi commit gồm code **và** test đi kèm. Chỉ `git add` đúng các file của commit đó, **không dùng `git add .`**.
6. Tên commit: `loại: mô tả tiếng Việt`. Loại gồm `docs`, `build`, `feat`, `test`, `fix`; phạm vi (nếu có) viết trong ngoặc, ví dụ `feat(common): ...`.
7. Mỗi tuần cả nhóm tối thiểu 5 commit và mỗi người đều phải có commit.

## Chuẩn bị môi trường (B làm một lần)

```bash
git clone https://github.com/dinhhuunhatminh/2026.1_CK_LTM
cd 2026.1_CK_LTM
sudo apt update
sudo apt install build-essential gdb gcovr    # trong WSL/Ubuntu
make test                                     # phải thấy "TẤT CẢ TEST ĐẠT"
```

Nên làm việc trong WSL (đường dẫn dạng `/mnt/d/...` nếu repo nằm trên ổ Windows).

## Bảng thứ tự hôm nay (15/11/2026)

| # | Người | Trạng thái | Tên commit | File |
|---|---|---|---|---|
| 1 | A – Minh | Xong (`80e87aa`) | `docs: thêm đề bài, tài liệu thiết kế, README và gitignore` | `De_bai.md`, `DESIGN.md`, `README.md`, `.gitignore` |
| 2 | A – Minh | Xong (`f9f5af8`) | `build: thêm khung dự án, Makefile và framework test minitest` | `Makefile`, `.gitattributes`, `tests/minitest.h`, `tests/unit/test_minitest.c`, các `.gitkeep` |
| 3 | A – Minh | Đang làm | `feat(common): thêm bộ đệm dòng linebuf xử lý truyền dòng TCP` | `include/linebuf.h`, `src/common/linebuf.c`, `tests/unit/test_linebuf.c`, `README.md`, `DESIGN.md`, `docs/THU_TU_COMMIT.md` |
| 4 | B – Cường | Chờ | `feat(common): thêm proto phân tích lệnh và mã phản hồi` | `include/proto.h`, `src/common/proto.c`, `tests/unit/test_proto.c` |
| 5 | B – Cường | Chờ | `feat(common): thêm util, simclock và log` | `include/util.h`, `include/simclock.h`, `include/log.h`, `src/common/{util,simclock,log}.c`, test tương ứng |
| 6 | B – Cường | Chờ | `feat(common): thêm netutil gồm tạo socket và send_all` | `include/netutil.h`, `src/common/netutil.c`, `tests/unit/test_netutil.c` |

B bắt đầu từ commit 4, sau khi A đã push commit 3 và cách đó ít nhất 30 phút.

## Gợi ý nội dung cho phần của B

Cách viết module và test: xem mẫu `include/linebuf.h`, `src/common/linebuf.c`, `tests/unit/test_linebuf.c`. Makefile tự nhận file `.c` mới trong `src/common/` và `tests/unit/test_*.c`, không cần sửa.

| Commit | Cần làm theo | Ý chính |
|---|---|---|
| 4 `proto` | DESIGN.md mục 3.1, 3.4–3.6 | Tách một dòng lệnh thành các trường (`CONNECT <app_id> <password>`, hoặc `<token> <LỆNH> [tham số]`); nhận diện lệnh hợp lệ; bảng mã phản hồi (100, 150, 200–206, 300, 999) và hàm dựng dòng phản hồi `<mã> <TEXT> [dữ liệu]`. Test cả ca sai: thiếu tham số, thừa tham số, lệnh lạ. |
| 5 `util`, `simclock`, `log` | DESIGN.md mục 1 (D6), 3.1, 4.4 | `util`: sinh chuỗi hex ngẫu nhiên từ `/dev/urandom`, cắt khoảng trắng, đổi chuỗi sang số có kiểm tra lỗi. `simclock`: đồng hồ giả lập có hệ số tăng tốc (`timescale`), nhận thời gian thật qua tham số để test được. `log`: ghi dòng `YYYY-MM-DD HH:MM:SS <NGUỒN> <nội dung>` ra file. |
| 6 `netutil` | DESIGN.md mục 3.2 | Hàm tạo socket TCP/UDP, `send_all` (lặp đến khi gửi đủ, xử lý `EINTR`, gửi một phần, dùng `MSG_NOSIGNAL`), đọc có timeout. Test trên `127.0.0.1` bằng `socketpair` hoặc cổng loopback. |

Mục tiêu coverage của mỗi module: trên 90 %. Kiểm tra bằng `make coverage`.

## Các tuần sau (dự kiến)

| Tuần | A – Minh | B – Cường |
|---|---|---|
| 2 | `session`, `store`, `server.c` (poll) | `appid`, `garden`, `scan`, `client` |
| 3 | `dev_sensor`, `dev_pump` | menu CLI, thiết lập thông số, điều khiển trực tiếp |
| 4 | `dev_fert`, `dev_light`, log | integration test, `loadtest.c` |
| 5 | bù coverage > 90 %, sửa lỗi | báo cáo, slide, số liệu chịu tải |
