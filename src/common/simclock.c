#include "simclock.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

int64_t simclock_real_ms(void) {
    struct timespec ts;

    if (timespec_get(&ts, TIME_UTC) == TIME_UTC) {
        return (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000L);
    }
    return (int64_t)time(NULL) * 1000;
}

int simclock_init(simclock_t *clk, int64_t real_now_ms,
                  int64_t sim_start_ms, double timescale) {
    if (clk == NULL || !isfinite(timescale) || timescale <= 0.0 || real_now_ms < 0) {
        return -1;
    }
    clk->start_real_ms = real_now_ms;
    clk->start_sim_ms  = (sim_start_ms > 0) ? sim_start_ms : real_now_ms;
    clk->timescale     = timescale;
    return 0;
}

int64_t simclock_now_ms(const simclock_t *clk, int64_t real_now_ms) {
    int64_t delta_real;

    if (clk == NULL) {
        return 0;
    }
    delta_real = real_now_ms - clk->start_real_ms;
    if (delta_real < 0) {
        delta_real = 0;
    }
    return clk->start_sim_ms + (int64_t)((double)delta_real * clk->timescale);
}

int64_t simclock_now_sec(const simclock_t *clk, int64_t real_now_ms) {
    return simclock_now_ms(clk, real_now_ms) / 1000;
}

int64_t simclock_to_real_ms(const simclock_t *clk, int64_t sim_delta_ms) {
    double scaled;

    if (clk == NULL || clk->timescale <= 0.0 || sim_delta_ms <= 0) {
        return 0;
    }
    scaled = ceil((double)sim_delta_ms / clk->timescale);
    return (int64_t)scaled;
}

int simclock_minute_of_day(int64_t sim_sec) {
    int64_t sec_in_day;

    if (sim_sec < 0) {
        return -1;
    }
    sec_in_day = sim_sec % 86400;
    return (int)(sec_in_day / 60);
}

/*
 * Đổi Unix timestamp (giây, >= 0) sang năm-tháng-ngày giờ:phút:giây UTC
 * theo thuật toán Howard Hinnant (không lệ thuộc gmtime_r/gmtime_s giữa POSIX và Windows).
 */
int simclock_format_time(int64_t sim_sec, char *buf, size_t sz) {
    int64_t days;
    int64_t rem;
    int64_t z;
    int64_t era;
    unsigned doe;
    unsigned yoe;
    int y;
    unsigned doy;
    unsigned mp;
    unsigned d;
    unsigned m;
    int hh;
    int mm;
    int ss;
    int n;

    if (buf == NULL || sz < 20 || sim_sec < 0) {
        return -1;
    }
    days = sim_sec / 86400;
    rem  = sim_sec % 86400;
    hh   = (int)(rem / 3600);
    mm   = (int)((rem % 3600) / 60);
    ss   = (int)(rem % 60);

    z   = days + 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (unsigned)(z - era * 146097);
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y   = (int)yoe + (int)era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp  = (5 * doy + 2) / 153;
    d   = doy - (153 * mp + 2) / 5 + 1;
    m   = mp < 10 ? mp + 3 : mp - 9;
    y  += (m <= 2);

    n = snprintf(buf, sz, "%04d-%02u-%02u %02d:%02d:%02d",
                 y, m, d, hh, mm, ss);
    return (n == 19) ? 0 : -1;
}
