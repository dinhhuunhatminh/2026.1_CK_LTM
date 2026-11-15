# Tài liệu thiết kế – Hệ thống mô phỏng chăm sóc vườn cây tự động

> Trạng thái: **bản nháp v0.1** để cả nhóm đọc và góp ý trước khi code.
> Ngôn ngữ: **C (C11, POSIX)**, build/chạy trên **WSL (Ubuntu)** bằng `gcc` + `make`.

---

## 1. Tổng quan

Hệ thống gồm hai loại chương trình, giao tiếp qua socket theo mô hình client–server:

| Chương trình | Vai trò | Binary |
|---|---|---|
| **Device simulator** | Mô phỏng 1 thiết bị (cảm biến / bơm tưới / bón phân / đèn). Chạy mỗi thiết bị là **một process riêng**, có ID riêng. | `build/device_sim` |
| **Controller** | Ứng dụng điều khiển của người dùng: quản lý vườn, quét, kết nối, điều khiển thiết bị. | `build/controller` |

```
                 UDP broadcast :9999  (SCAN)
  ┌────────────┐ ───────────────────────────────►  ┌───────────────┐
  │            │ ◄───────────────────────────────  │ device_sim    │ sensor  S01  tcp:7001
  │ controller │        DEVICE <id> <type> <port>  ├───────────────┤
  │            │                                   │ device_sim    │ pump    P01  tcp:7002
  │ (CLI menu) │ ═══════ TCP, giao thức text ════► ├───────────────┤
  │            │   CONNECT / SET / GET / PUMP ...  │ device_sim    │ fert    F01  tcp:7003
  └────────────┘                                   ├───────────────┤
                                                   │ device_sim    │ light   L01  tcp:7004
                                                   └───────────────┘
       bơm / bón phân ──(TCP, như một client)──► cảm biến   (hỏi độ ẩm/NPK, báo lượng nước/phân đã cấp)
```

### Quyết định thiết kế chính (và lý do)

| # | Quyết định | Lý do |
|---|---|---|
| D1 | **Quét thiết bị bằng UDP broadcast**, kết nối/điều khiển bằng **TCP** | Khớp đề ("lắng nghe thông điệp quét"), khớp chương 2 giáo trình (UDP sender/receiver + TCP server/client). |
| D2 | **Giao thức text, mỗi lệnh một dòng kết thúc `\n`** | Theo mẫu chat protocol ở mục 4.3 giáo trình; dễ debug bằng `nc`; dễ viết unit test. |
| D3 | Server dùng **một luồng + `poll()`** (event loop), không fork/thread | Một vòng lặp xử lý được UDP scan, TCP accept, TCP client, và timer (lịch tưới, chu kỳ cảm biến). Không cần khóa. Giáo trình mục 3.5 (multiplexing). |
| D4 | **Mọi lệnh sau khi kết nối đều mang token**: `<token> <LỆNH> ...` | Đề yêu cầu token gắn với ID ứng dụng để quản lý ứng dụng đã xác thực. Mang token mỗi lệnh → client có thể ngắt và nối lại TCP mà không phải đăng nhập lại. |
| D5 | **Tách logic thuần khỏi I/O** (parser, auth, lịch, quy tắc tưới/bón/đèn nhận thời gian & dữ liệu qua tham số) | Để đạt coverage > 90% bằng unit test không cần socket. Phần socket mỏng, được phủ bằng integration test trên loopback. |
| D6 | **Đồng hồ giả lập có hệ số tăng tốc** (`--timescale`) | Lịch "tưới 6:00 sáng", chu kỳ T phút... không thể chờ thật khi test. Test và demo chạy nhanh 60×–3600×. |
| D7 | **Bơm/bón phân tự hỏi cảm biến qua TCP** (như một client bình thường), cảm biến mô phỏng đất | Hệ thống phải tự động chạy kể cả khi controller tắt. Đóng được vòng lặp mô phỏng: khô → tưới → ẩm lên. |
| D8 | Trạng thái thiết bị (mật khẩu, tham số, token) lưu file; log ghi file riêng từng thiết bị | Đổi mật khẩu phải bền qua lần khởi động lại; yêu cầu "ghi log trên mỗi thiết bị". |

---

## 2. Cấu trúc thư mục

```
FinalProject/
├── Makefile
├── DESIGN.md
├── README.md
├── include/                 # header công khai của từng module
├── src/
│   ├── common/
│   │   ├── linebuf.c        # đệm dòng cho TCP stream (xử lý truyền dòng)
│   │   ├── proto.c          # parse lệnh / dựng phản hồi, bảng mã lỗi
│   │   ├── netutil.c        # tạo socket, send_all, đọc có timeout
│   │   ├── simclock.c       # đồng hồ giả lập (timescale)
│   │   ├── log.c            # ghi log có timestamp ra file
│   │   └── util.c           # random hex, trim, parse số có kiểm tra
│   ├── device/
│   │   ├── main.c           # đọc tham số dòng lệnh, khởi tạo
│   │   ├── server.c         # event loop poll(): UDP + TCP + timer
│   │   ├── session.c        # bảng ứng dụng đã xác thực (app_id ↔ token)
│   │   ├── store.c          # lưu/đọc trạng thái (mật khẩu, tham số)
│   │   ├── dev_sensor.c     # logic + mô hình đất
│   │   ├── dev_pump.c       # logic bơm tưới
│   │   ├── dev_fert.c       # logic bón phân NPK
│   │   └── dev_light.c      # logic đèn quang hợp
│   └── controller/
│       ├── main.c           # menu CLI
│       ├── garden.c         # quản lý vườn + danh sách thiết bị, lưu file
│       ├── scan.c           # UDP broadcast + thu phản hồi
│       ├── client.c         # kết nối TCP tới thiết bị, gửi lệnh, đọc phản hồi
│       └── appid.c          # sinh/lưu ID ứng dụng ngẫu nhiên
├── tests/
│   ├── minitest.h           # framework test tối giản (không phụ thuộc thư viện ngoài)
│   ├── unit/                # test từng module, không dùng mạng
│   ├── integration/         # chạy device thật trên 127.0.0.1, controller API thật
│   └── load/
│       └── loadtest.c       # tạo N client đồng thời bằng pthread, báo cáo
├── docs/                    # báo cáo, slide, test case
└── logs/ data/              # sinh ra khi chạy (đưa vào .gitignore)
```

Các module trong `common/` và `device/dev_*.c` **không gọi trực tiếp** `time()`/`socket()`; thời gian và dữ liệu cảm biến được truyền vào qua tham số → test được bằng giá trị cho trước.

---

## 3. Giao thức

### 3.1. Quy ước chung

- Văn bản ASCII, **mỗi thông điệp một dòng**, kết thúc bằng `\n` (chấp nhận `\r\n`). Tối đa **512 byte/dòng**; dài hơn → `202 BAD ARGUMENT` và bỏ phần thừa đến khi gặp `\n`.
- Các trường cách nhau **một dấu cách**. Mật khẩu / ID / token **không chứa khoảng trắng**.
- Số thực dùng dấu chấm (`12.5`), giờ dùng `HH:MM` 24 giờ.
- **ID thiết bị**: 1–16 ký tự `[A-Za-z0-9_-]`, duy nhất trong hệ thống (cấu hình lúc khởi động). **ID ứng dụng (controller)**: 16 ký tự hex ngẫu nhiên, sinh lần chạy đầu và lưu file.
- **Token**: 16 ký tự hex, lấy từ `/dev/urandom`.

### 3.2. Xử lý truyền dòng (TCP stream)

TCP là dòng byte, không giữ ranh giới thông điệp: một `recv()` có thể trả nửa dòng, hoặc nhiều dòng dính nhau. Mỗi kết nối có một `linebuf`:

```
recv() → append vào buf → lặp: tìm '\n' → cắt ra 1 dòng → xử lý → dịch phần còn lại lên đầu
```

Gửi dùng `send_all()` (lặp đến khi gửi đủ, xử lý `EINTR`/gửi một phần, dùng `MSG_NOSIGNAL`). Đây là phần "Xử lý truyền dòng" và được unit test với các ca: nửa dòng, nhiều dòng một lần, dòng quá dài, `\r\n`, byte rỗng.

### 3.3. Quét thiết bị (UDP, cổng 9999)

Mọi device bind UDP `:9999` với `SO_REUSEADDR` để nhiều thiết bị cùng máy cùng nghe.

```
controller → broadcast :  SCAN <app_id>\n
device     → unicast về : DEVICE <device_id> <type> <tcp_port>\n
```

`type` ∈ `SENSOR | PUMP | FERT | LIGHT`. Thông điệp sai định dạng bị bỏ qua (không trả lời).
Controller gom phản hồi trong **2 giây**, hiển thị loại + ID, người dùng chọn thêm vào vườn.

> **Rủi ro WSL2:** broadcast `255.255.255.255` giữa các process cùng máy có thể không tới do mạng ảo hóa. Cách chắc ăn: gửi SCAN tới cả `255.255.255.255` **và** `127.0.0.1:9999`. Sẽ kiểm chứng ngay ở commit đầu tiên có mạng; nếu cần sẽ thêm tùy chọn `--scan-host`.

### 3.4. Kết nối & xác thực (TCP)

```
C: CONNECT <app_id> <password>
S: 100 OK <token> <device_id> <type> <version>      (đúng mật khẩu)
S: 200 BAD PASSWORD                                  (sai mật khẩu)
```

- Thiết bị tạo token ngẫu nhiên, lưu `app_id → token`. Nếu `app_id` đã có token thì **cấp lại token cũ** (kết nối lại không làm mất phiên).
- Tối đa **16 ứng dụng** xác thực đồng thời/thiết bị; vượt quá → `205 TOO MANY APPS`.
- Sai mật khẩu liên tiếp **5 lần** từ cùng `app_id` → khóa 30 giây (giả lập thời gian), trả `206 LOCKED`. (Chống dò mật khẩu.)
- Mọi lệnh khác dạng: `<token> <LỆNH> [tham số...]`. Token sai/thiếu → `201 BAD TOKEN`. Đổi mật khẩu thành công sẽ **thu hồi mọi token khác** (trừ phiên đang đổi).

### 3.5. Phản hồi

`<mã> <TEXT> [dữ liệu dạng key=value ...]\n`

| Mã | Ý nghĩa |
|---|---|
| 100 | OK |
| 150 | OK, theo sau là N dòng dữ liệu (dùng cho `LOG`) — dòng đầu: `150 LOG <N>` |
| 200 | BAD PASSWORD |
| 201 | BAD TOKEN |
| 202 | BAD ARGUMENT (thiếu / sai kiểu / ngoài khoảng cho phép) |
| 203 | UNSUPPORTED (lệnh không áp dụng cho loại thiết bị này) |
| 204 | DEVICE OFF (thiết bị đang tắt, không nhận lệnh vận hành) |
| 205 | TOO MANY APPS |
| 206 | LOCKED |
| 300 | UNKNOWN COMMAND |
| 999 | UNKNOWN ERROR |

### 3.6. Bảng lệnh

**Lệnh chung (mọi loại thiết bị)**

| Lệnh | Tác dụng | Phản hồi thành công |
|---|---|---|
| `INFO` | Thông tin thiết bị | `100 OK id=.. type=.. version=.. uptime=..` |
| `STATUS` | Trạng thái hiện tại | `100 OK power=ON\|OFF ...` (tuỳ loại) |
| `PARAMS` | Toàn bộ tham số đang cài | `100 OK k1=v1 k2=v2 ...` |
| `POWER ON\|OFF` | Bật/tắt thiết bị | `100 OK` |
| `PASSWD <cũ> <mới>` | Đổi mật khẩu (mới: 4–32 ký tự) | `100 OK` / `200 BAD PASSWORD` |
| `LOG [n]` | n dòng log gần nhất (mặc định 20) | `150 LOG <N>` + N dòng |

**Cảm biến (`SENSOR`)**

| Lệnh | Tác dụng |
|---|---|
| `SET INTERVAL <T>` | Chu kỳ ghi dữ liệu T phút (1–1440) |
| `DATA` | Lần đo mới nhất: `100 OK ts=.. moisture=.. N=.. P=.. K=..` |
| `HISTORY [n]` | n lần đo gần nhất (`150 HISTORY <N>` + dòng) |
| `FEED WATER <lít>` | *(do bơm gọi)* cộng nước vào đất |
| `FEED NPK <gam_N> <gam_P> <gam_K>` | *(do bộ bón phân gọi)* cộng dinh dưỡng vào đất |

**Bộ tưới (`PUMP`)**

| Lệnh | Tác dụng |
|---|---|
| `SET HMAX <%>` , `SET HMIN <%>` | Ngưỡng độ ẩm (ràng buộc `0 ≤ Hmin < Hmax ≤ 100`) |
| `SET SCHEDULE <HH:MM[,HH:MM...]\|NONE>` | Các giờ tưới cố định trong ngày |
| `SET VOLUME <lít>` | Lượng nước mỗi lần tưới |
| `SET SENSOR <host> <port> <mật_khẩu>` | Cảm biến để đọc độ ẩm |
| `PUMP <lít>` | **Điều khiển trực tiếp**: tưới ngay, không phụ thuộc lịch/ngưỡng |

**Bộ bón phân (`FERT`)**

| Lệnh | Tác dụng |
|---|---|
| `SET NMIN <v>` , `SET PMIN <v>` , `SET KMIN <v>` | Ngưỡng dinh dưỡng |
| `SET CONC <C>` | Nồng độ phân hòa nước, gam/lít |
| `SET VOLUME <V>` | Lượng nước tưới kèm, lít |
| `SET NPK_RATIO <n>:<p>:<k>` | Tỉ lệ N:P:K trong phân (mặc định 1:1:1) |
| `SET SENSOR <host> <port> <mật_khẩu>` | Cảm biến để đọc NPK |
| `FERTILIZE` | **Điều khiển trực tiếp**: bón ngay một lần |

**Đèn quang hợp (`LIGHT`)**

| Lệnh | Tác dụng |
|---|---|
| `SET WATT <P>` | Công suất phát sáng (0–400 W) |
| `SET SCHEDULE <HH:MM-HH:MM[,...]\|NONE>` | Khung giờ bật (cho phép qua nửa đêm, vd `22:00-02:00`) |
| `LIGHT ON\|OFF` | **Điều khiển trực tiếp**, ghi đè lịch cho tới mốc lịch tiếp theo |

### 3.7. Ví dụ một phiên

```
C: CONNECT 9f3a1c0b7d2e4a58 1234
S: 100 OK 5b1e77a09c3d4f21 P01 PUMP 1.0
C: 5b1e77a09c3d4f21 SET HMIN 30
S: 100 OK
C: 5b1e77a09c3d4f21 SET SCHEDULE 06:00,17:30
S: 100 OK
C: 5b1e77a09c3d4f21 PUMP 2.5
S: 100 OK watered=2.5
C: 5b1e77a09c3d4f21 SET HMIN 120
S: 202 BAD ARGUMENT hmin must be in [0,hmax)
C: 5b1e77a09c3d4f21 LOG 2
S: 150 LOG 2
S: 2026-11-15 06:00:00 AUTO schedule watered 2.5L moisture=41.2
S: 2026-11-15 09:13:20 CMD app=9f3a1c0b PUMP 2.5L
```

Có thể thử thủ công bằng `nc 127.0.0.1 7002` rồi gõ lệnh.

---

## 4. Thiết kế phía thiết bị

### 4.1. Khởi tạo thông số (1đ)

```
device_sim --type pump --id P01 --port 7002 --password 1234 [--timescale 60] [--state-dir data/] [--log-dir logs/]
```

Giá trị mặc định mỗi loại (có thể sửa bằng lệnh `SET`):

| Loại | Mặc định |
|---|---|
| SENSOR | T = 5 phút; đất khởi tạo ẩm 60 %, N=P=K=50 |
| PUMP | Hmin 30 %, Hmax 70 %, lịch `06:00,17:00`, 2 lít/lần |
| FERT | Nmin=Pmin=Kmin=20, C=5 g/l, V=1 lít |
| LIGHT | 100 W, lịch `06:00-10:00` |

Nếu có file trạng thái trong `--state-dir` (`<id>.state`) thì **ưu tiên giá trị trong file** (mật khẩu đã đổi, tham số đã chỉnh).

### 4.2. Event loop (`server.c`)

Một `poll()` theo dõi: socket UDP scan, socket TCP listen, các socket TCP client. `timeout` của `poll` = thời gian tới tick kế tiếp (tối đa 1 giây thật). Mỗi vòng:

1. Xử lý fd sẵn sàng: UDP → trả lời SCAN; listen → `accept` (non-blocking); client → `recv` → `linebuf` → từng dòng đi qua `proto_parse` → handler theo loại thiết bị → `send_all`.
2. Nếu tới hạn → `device_tick(now_sim)`: chạy các quy tắc tự động bên dưới.
3. Đóng kết nối khi client ngắt / lỗi / im lặng quá 120 giây.

Tối đa 64 kết nối đồng thời mỗi thiết bị (mảng `struct pollfd` cố định).

### 4.3. Quy tắc tự động (hàm thuần `*_tick`)

**Cảm biến** – mỗi T phút giả lập: cập nhật mô hình đất rồi lưu một mẫu vào vòng đệm 256 mẫu.
Mô hình đất đơn giản: độ ẩm giảm ~0.5 %/giờ, N/P/K giảm ~0.2/giờ; `FEED WATER x` làm ẩm tăng `x*8 %` (giới hạn 100); `FEED NPK` cộng dồn (giới hạn 100).

**Bơm tưới** – cả hai cơ chế chạy song song, mỗi tick:

```
h = độ ẩm đọc từ cảm biến
(a) nếu đúng một mốc trong SCHEDULE (mỗi mốc chỉ kích hoạt 1 lần/ngày) và h <= Hmax  → tưới VOLUME lít
(b) nếu h < Hmin                                                                    → tưới VOLUME lít
sau mỗi lần tưới: FEED WATER tới cảm biến, ghi log
chống tưới liên tục: giữa 2 lần tự động tối thiểu 10 phút giả lập
```

**Bón phân** – nếu `N < Nmin` hoặc `P < Pmin` hoặc `K < Kmin`: bón một lần với nồng độ `C` g/l và `V` lít nước; lượng phân = `C*V` gam chia theo `NPK_RATIO`; gọi `FEED NPK` + `FEED WATER V` tới cảm biến. Cũng có cooldown 10 phút giả lập.

**Đèn** – `light_is_on(now, schedule)` kiểm tra giờ hiện tại thuộc khung nào (xử lý khung qua nửa đêm). Khi trạng thái đổi → ghi log. Lệnh `LIGHT ON|OFF` đặt `override` đến khi chuyển mốc lịch tiếp theo.

Nếu thiết bị `POWER OFF`: không chạy quy tắc tự động, từ chối lệnh vận hành bằng `204`, nhưng vẫn trả lời `INFO`, `STATUS`, `PARAMS`, `LOG`, `POWER`, `PASSWD`.
Nếu bơm/bón phân không liên lạc được cảm biến: ghi log `WARN sensor unreachable`, bỏ qua tick, thử lại tick sau (không chặn quá 1 giây nhờ timeout kết nối).

### 4.4. Log (1đ)

Mỗi thiết bị ghi `logs/<device_id>.log`, mỗi dòng:

```
YYYY-MM-DD HH:MM:SS <NGUỒN> <nội dung>
```

`NGUỒN` ∈ `AUTH` (kết nối, sai mật khẩu, đổi mật khẩu), `CMD` (lệnh từ controller, kèm `app=<8 ký tự đầu app_id>`), `AUTO` (hành động tự động), `SYS` (khởi động/tắt), `WARN`.
Lệnh `LOG [n]` đọc lại n dòng cuối của file này.

---

## 5. Thiết kế Controller

### 5.1. Chức năng (menu CLI)

```
=== Garden Controller (app_id: 9f3a1c0b7d2e4a58) ===
1. Quản lý vườn      (thêm / sửa / xóa / liệt kê / chi tiết)
2. Quét thiết bị     (UDP broadcast, thêm vào vườn đang chọn)
3. Thiết bị trong vườn → chọn thiết bị:
      a. Kết nối (nhập mật khẩu)      b. Đổi mật khẩu
      c. Bật/Tắt                       d. Xem thông tin / thông số / trạng thái / dữ liệu
      e. Thiết lập thông số            f. Điều khiển trực tiếp (tưới / bón / đèn)
      g. Xem log thiết bị              h. Xóa thiết bị khỏi vườn
0. Thoát
```

Menu chỉ là lớp mỏng; mọi thao tác nằm trong hàm của `scan.c`, `client.c`, `garden.c` để test được không cần giao diện.

### 5.2. Quản lý vườn (1đ)

Mô hình dữ liệu:

```
Garden { id, name, crop (loại cây), area (m²), note, devices[ ≤ 32 ] }
Device { device_id, type, host, port, token (có thể rỗng) }
```

Lưu trong `data/controller/gardens.txt` (text, mỗi vườn một block) và `data/controller/app_id.txt`. Token được lưu để lần chạy sau dùng lại, nếu bị từ chối (`201`) thì hỏi mật khẩu để `CONNECT` lại. Mật khẩu thiết bị **không** lưu.

### 5.3. Client TCP

`client.c` cung cấp API: `dev_connect()`, `dev_send_cmd(&conn, "SET HMIN 30", &reply)`, `dev_read_reply()` (dùng chung `linebuf`, hiểu phản hồi `150` nhiều dòng), timeout 3 giây mỗi lệnh. Chương trình hiển thị phản hồi thân thiện (dịch mã lỗi sang tiếng Việt).

---

## 6. Kiểm thử

### 6.1. Unit test (mục tiêu coverage ≥ 90 %)

- Framework: `tests/minitest.h` (macro `ASSERT_EQ`, `ASSERT_STR_EQ`, …), không cần cài thêm thư viện.
- Đo coverage: `gcc --coverage` + `gcovr` (hoặc `lcov`). Lệnh: `make coverage` → `build/coverage/index.html`.
- Các nhóm test: `linebuf` (ranh giới dòng), `proto` (parse hợp lệ/không hợp lệ cho từng lệnh), `session` (token, giới hạn, khóa 30s), `dev_*` (từng nhánh quy tắc tưới/bón/đèn với thời gian & giá trị cho trước), `store` (lưu/đọc/hỏng file), `garden` (CRUD + lưu file), `simclock`.
- **Integration test** chạy device thật trên `127.0.0.1` với cổng ngẫu nhiên, controller API thật: scan → connect → set → get → đổi mật khẩu → log. Những test này phủ phần `server.c`, `netutil.c`, `scan.c`, `client.c` nên được tính vào coverage chung.

### 6.2. Test chịu tải (`tests/load/loadtest.c`)

Dùng pthread tạo N client (mặc định 100, tăng dần 100 → 500 → 1000) đồng thời tới một device:
- mỗi client: `CONNECT` + 50 lệnh `STATUS/PARAMS/SET` xen kẽ;
- thêm kịch bản "kết nối rồi gửi từng byte một" và "gửi nhiều lệnh dính nhau" để kiểm tra stream;
- báo cáo: tổng lệnh/giây, độ trễ trung bình / p95 / max, số lỗi, số kết nối bị từ chối.

Kết quả (bảng + biểu đồ) đưa vào báo cáo. Giới hạn 64 kết nối đồng thời sẽ được **đo và ghi nhận**; nếu quá thấp thì nâng theo kết quả test (đổi sang mảng cấp phát động).

---

## 7. Đối chiếu thang điểm

| Yêu cầu | Điểm | Nằm ở đâu |
|---|---|---|
| Xử lý truyền dòng | 1 | §3.2 `linebuf.c`, `send_all` |
| Cơ chế vào/ra socket trên server | 2 | §4.2 `server.c` – `poll()` đa kết nối + UDP + timer |
| Xác thực truy cập điều khiển | 1 | §3.4 CONNECT, token, khóa chống dò |
| Khởi tạo các thông số cho thiết bị | 1 | §4.1 tham số dòng lệnh, mặc định, file trạng thái |
| Quét thiết bị | 2 | §3.3 UDP broadcast + §5.1 menu quét/thêm vào vườn |
| Kết nối thiết bị | 2 | §3.4 + §5.3 |
| Đổi mật khẩu | 1 | `PASSWD`, lưu bền, thu hồi token |
| Điều khiển bật tắt | 1 | `POWER ON\|OFF` |
| Thông số bộ tưới nước | 1 | `SET HMAX/HMIN/SCHEDULE/VOLUME` |
| Thông số bộ bón phân | 2 | `SET NMIN/PMIN/KMIN/CONC/VOLUME/NPK_RATIO` |
| Thông số đèn quang hợp | 1 | `SET WATT/SCHEDULE` |
| Điều khiển trực tiếp bên cạnh lịch | 1 | `PUMP`, `FERTILIZE`, `LIGHT` |
| Lấy thông tin/thông số mỗi loại thiết bị | 2 | `INFO`, `STATUS`, `PARAMS`, `DATA`, `HISTORY` |
| Ghi log trên mỗi thiết bị | 1 | §4.4 + lệnh `LOG` |
| Quản lý thông tin vườn cây | 1 | §5.2 |

---

## 8. Phân công & kế hoạch commit

Nhóm **2 người**: **A** và **B**. Phạm vi kiến thức: đã học tới **ghép kênh (select/poll)**, nên dùng `poll()` như thiết kế; **không** dùng fork/thread cho server (thread chỉ dùng trong chương trình test tải), **không** làm TLS/OpenSSL.

### 8.1. Phân công dài hạn

| | Người A | Người B |
|---|---|---|
| Nền tảng | tài liệu, `.gitignore`, `linebuf`, `util`, `simclock`, `log` | khung dự án, `Makefile`, `minitest.h`, `proto`, `netutil` |
| Phía thiết bị | `session`, `store`, `server.c` (poll), `dev_sensor`, `dev_pump`, `dev_fert`, `dev_light` | – |
| Phía controller | – | `appid`, `garden`, `scan`, `client`, menu CLI |
| Kiểm thử | unit test các module của mình, integration test phía device | unit test các module của mình, integration test controller, `loadtest.c` |
| Tài liệu | báo cáo: chương giao thức & thiết bị | báo cáo: chương controller & kiểm thử; slide chia đôi |

Quy ước: mỗi người commit bằng **tài khoản git của chính mình**, mỗi commit nhỏ, gồm code + test đi kèm, `make test` phải chạy xanh trước khi commit. Làm việc trên nhánh riêng, merge vào `main` qua pull request để người kia xem.

### 8.2. Hôm nay: 6 commit (mỗi người 3), xen kẽ, cách nhau ≥ 30 phút

| Thứ tự | Người | Nội dung |
|---|---|---|
| 1 | A | `docs`: `De_bai.md`, `DESIGN.md`, `.gitignore`, `README.md` khung |
| 2 | B | Khung dự án: thư mục, `Makefile` (build/test/coverage), `tests/minitest.h`, 1 test mẫu chạy trên WSL |
| 3 | A | `common/linebuf` + unit test (xử lý truyền dòng) |
| 4 | B | `common/proto` (parse lệnh, bảng mã phản hồi) + unit test |
| 5 | A | `common/util`, `simclock`, `log` + unit test |
| 6 | B | `common/netutil` (tạo socket, `send_all`, đọc có timeout) + test loopback |

Commit 1 trước, vì commit 2 cần thư mục và `.gitignore` từ commit 1. Các module hôm nay độc lập nhau nên hai người làm song song được, chỉ cần đẩy commit đúng thứ tự thời gian.

### 8.3. Các tuần sau (dự kiến)

| Tuần | A | B |
|---|---|---|
| 2 | `session`, `store`, `server.c` (poll: UDP + TCP + timer), lệnh chung | `appid`, `garden`, `scan` (UDP), `client` |
| 3 | `dev_sensor`, `dev_pump` | menu CLI, thiết lập thông số, điều khiển trực tiếp |
| 4 | `dev_fert`, `dev_light`, log | integration test controller ↔ device, `loadtest.c` |
| 5 | bù coverage > 90 %, sửa lỗi | báo cáo, slide, số liệu chịu tải |

---

## 9. Đã chốt / còn mở

**Đã chốt:** nhóm 2 người; dùng WSL; `poll()` đơn luồng; không làm TLS.

**Còn mở (chưa ảnh hưởng commit hôm nay):**
1. Khoảng giá trị và mặc định ở §4.1 có cần theo yêu cầu cụ thể của giảng viên không?
2. Chấp nhận việc bơm/bón phân tự gọi cảm biến (D7)? Nếu thấy phức tạp, phương án đơn giản hơn là cho controller đọc cảm biến rồi ra lệnh, nhưng khi đó hệ thống không tự động nếu tắt controller.
