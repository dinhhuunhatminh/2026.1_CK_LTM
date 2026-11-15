/*
 * simclock.h - đồng hồ giả lập có hệ số tăng tốc (timescale).
 *
 * Mục đích (DESIGN.md mục 1 D6):
 *   - Các quy tắc tự động (chu kỳ cảm biến T phút, lịch tưới 06:00, lịch đèn...)
 *     nhận thời gian giả lập qua tham số để có thể chạy nhanh gấp 60x-3600x khi
 *     demo và kiểm thử tức thời trong unit test mà không cần sleep thật.
 */
#ifndef SIMCLOCK_H
#define SIMCLOCK_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int64_t start_real_ms; /* mốc thời gian thật lúc khởi tạo (ms từ epoch) */
    int64_t start_sim_ms;  /* mốc thời gian giả lập lúc khởi tạo (ms từ epoch) */
    double  timescale;     /* hệ số tăng tốc (> 0, mặc định 1.0) */
} simclock_t;

/* Lấy thời gian thực hiện tại của hệ thống tính bằng mili-giây từ Unix epoch. */
int64_t simclock_real_ms(void);

/*
 * Khởi tạo đồng hồ giả lập.
 *   real_now_ms  : mốc thời gian thật ban đầu (ms).
 *   sim_start_ms : mốc thời gian giả lập ban đầu (ms); nếu <= 0 sẽ lấy bằng real_now_ms.
 *   timescale    : hệ số tăng tốc (> 0.0).
 * Trả về 0 nếu thành công, -1 nếu clk == NULL hoặc timescale <= 0.
 */
int simclock_init(simclock_t *clk, int64_t real_now_ms,
                  int64_t sim_start_ms, double timescale);

/* Tính thời gian giả lập hiện tại (mili-giây) ứng với thời điểm thật real_now_ms. */
int64_t simclock_now_ms(const simclock_t *clk, int64_t real_now_ms);

/* Tính thời gian giả lập hiện tại (giây) ứng với thời điểm thật real_now_ms. */
int64_t simclock_now_sec(const simclock_t *clk, int64_t real_now_ms);

/*
 * Đổi một khoảng thời gian giả lập sim_delta_ms sang thời gian chờ thực tế (ms)
 * để truyền vào poll() timeout. Luôn >= 0.
 */
int64_t simclock_to_real_ms(const simclock_t *clk, int64_t sim_delta_ms);

/*
 * Lấy số phút trong ngày (0..1439, tính theo UTC) từ giây giả lập sim_sec.
 * Trả về -1 nếu sim_sec < 0.
 */
int simclock_minute_of_day(int64_t sim_sec);

/*
 * Định dạng giây giả lập sim_sec thành chuỗi "YYYY-MM-DD HH:MM:SS" (UTC).
 * Bộ đệm buf cần ít nhất 20 byte. Trả về 0 nếu thành công, -1 nếu lỗi.
 */
int simclock_format_time(int64_t sim_sec, char *buf, size_t sz);

#endif /* SIMCLOCK_H */
