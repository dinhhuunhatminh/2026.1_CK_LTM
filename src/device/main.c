/* device_sim - chương trình mô phỏng một thiết bị (cảm biến, bơm, bón phân, đèn). */
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "device.h"
#include "options.h"
#include "server.h"
#include "simclock.h"

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

int main(int argc, char **argv) {
    dev_options_t opt;
    device_t dev;
    server_t srv;
    struct sigaction sa;
    char err[256] = "";
    int rc;

    rc = options_parse(argc, argv, &opt, err, sizeof err);
    if (rc != 0) {
        if (rc < 0) {
            fprintf(stderr, "Lỗi: %s\n\n", err);
        }
        options_usage(rc < 0 ? stderr : stdout, argv[0]);
        return rc < 0 ? 2 : 0;
    }
    if (device_init(&dev, &opt.cfg, simclock_real_ms(), err, sizeof err) != 0) {
        fprintf(stderr, "Lỗi: %s\n", err);
        return 1;
    }
    if (server_open(&srv, &dev, opt.bind_ip, opt.port, opt.scan_port) != 0) {
        fprintf(stderr, "Lỗi: không mở được cổng TCP %d hoặc UDP %d (đang bị chiếm?)\n",
                opt.port, opt.scan_port);
        return 1;
    }

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal; /* không đặt SA_RESTART để poll() thoát khi nhận tín hiệu */
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("Thiết bị %s (%s) đang chạy: TCP %d, quét UDP %d. Nhấn Ctrl+C để dừng.\n",
           dev.id, proto_dev_type_str(dev.type), srv.tcp_port, srv.scan_port);
    fflush(stdout);

    while (!g_stop) {
        if (server_step(&srv, 1000) != 0) {
            fprintf(stderr, "Lỗi vòng lặp socket, dừng thiết bị\n");
            break;
        }
    }
    device_log(&dev, simclock_real_ms(), LOG_SRC_SYS, "stopped");
    server_close(&srv);
    return 0;
}
