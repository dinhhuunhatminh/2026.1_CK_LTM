#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <time.h>
#endif

static void bytes_to_hex(const unsigned char *raw, char *buf, size_t hex_len) {
    static const char hex_digits[] = "0123456789abcdef";
    size_t i;

    for (i = 0; i < hex_len; i++) {
        unsigned char b = raw[i / 2];
        unsigned char nibble = (i % 2 == 0) ? (unsigned char)(b >> 4)
                                            : (unsigned char)(b & 0x0fU);
        buf[i] = hex_digits[nibble];
    }
    buf[hex_len] = '\0';
}

int util_random_hex_from_file(const char *path, char *buf, size_t hex_len) {
    unsigned char raw[64];
    size_t nbytes;
    FILE *fp;
    size_t got;

    if (path == NULL || buf == NULL || hex_len == 0 || hex_len > sizeof(raw) * 2) {
        return -1;
    }
    nbytes = (hex_len + 1) / 2;
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    got = fread(raw, 1, nbytes, fp);
    fclose(fp);
    if (got != nbytes) {
        return -1;
    }
    bytes_to_hex(raw, buf, hex_len);
    return 0;
}

int util_random_hex(char *buf, size_t hex_len) {
    if (util_random_hex_from_file("/dev/urandom", buf, hex_len) == 0) {
        return 0;
    }
#ifdef _WIN32
    /* Dự phòng khi chạy unit test trực tiếp trên Windows không có /dev/urandom. */
    if (buf != NULL && hex_len > 0 && hex_len <= 128) {
        unsigned char raw[64];
        size_t nbytes = (hex_len + 1) / 2;
        static unsigned int seeded = 0;
        size_t i;

        if (!seeded) {
            srand((unsigned int)time(NULL) ^ 0xA5A55A5AU);
            seeded = 1;
        }
        for (i = 0; i < nbytes; i++) {
            raw[i] = (unsigned char)((rand() ^ (rand() << 4)) & 0xFF);
        }
        bytes_to_hex(raw, buf, hex_len);
        return 0;
    }
#endif
    return -1;
}

char *util_trim(char *s) {
    char *start;
    char *end;
    size_t len;

    if (s == NULL) {
        return NULL;
    }
    start = s;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }
    len = strlen(start);
    end = start + len;
    while (end > start && isspace((unsigned char)end[-1])) {
        end--;
    }
    len = (size_t)(end - start);
    if (start != s && len > 0) {
        memmove(s, start, len);
    }
    s[len] = '\0';
    return s;
}

int util_str_copy(char *dst, size_t dst_sz, const char *src) {
    size_t len;

    if (dst == NULL || dst_sz == 0 || src == NULL) {
        return -1;
    }
    len = strlen(src);
    if (len >= dst_sz) {
        memcpy(dst, src, dst_sz - 1);
        dst[dst_sz - 1] = '\0';
        return -1;
    }
    memcpy(dst, src, len + 1);
    return 0;
}

int util_valid_id(const char *s, size_t max_len) {
    size_t i;

    if (s == NULL || max_len == 0 || s[0] == '\0') {
        return 0;
    }
    for (i = 0; s[i] != '\0'; i++) {
        unsigned char c = (unsigned char)s[i];
        if (i >= max_len) {
            return 0;
        }
        if (!(isalnum(c) || c == '_' || c == '-')) {
            return 0;
        }
    }
    return 1;
}

int util_valid_hex(const char *s, size_t hex_len) {
    size_t i;

    if (s == NULL || hex_len == 0) {
        return 0;
    }
    for (i = 0; i < hex_len; i++) {
        if (s[i] == '\0' || !isxdigit((unsigned char)s[i])) {
            return 0;
        }
    }
    return s[hex_len] == '\0';
}

int util_parse_int(const char *s, int min_val, int max_val, int *out) {
    char *end = NULL;
    long v;

    if (s == NULL || out == NULL || min_val > max_val) {
        return -1;
    }
    if (*s == '\0' || isspace((unsigned char)*s)) {
        return -1;
    }
    errno = 0;
    v = strtol(s, &end, 10);
    if (errno == ERANGE || end == s || *end != '\0') {
        return -1;
    }
    if (v < (long)min_val || v > (long)max_val) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

int util_parse_double(const char *s, double min_val, double max_val, double *out) {
    char *end = NULL;
    double v;

    if (s == NULL || out == NULL || min_val > max_val) {
        return -1;
    }
    if (*s == '\0' || isspace((unsigned char)*s)) {
        return -1;
    }
    errno = 0;
    v = strtod(s, &end);
    if (errno == ERANGE || end == s || *end != '\0' || !isfinite(v)) {
        return -1;
    }
    if (v < min_val || v > max_val) {
        return -1;
    }
    *out = v;
    return 0;
}

int util_parse_hhmm(const char *s, int *minutes_out) {
    int hh;
    int mm;

    if (s == NULL || minutes_out == NULL || strlen(s) != 5 || s[2] != ':') {
        return -1;
    }
    if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1]) ||
        !isdigit((unsigned char)s[3]) || !isdigit((unsigned char)s[4])) {
        return -1;
    }
    hh = (s[0] - '0') * 10 + (s[1] - '0');
    mm = (s[3] - '0') * 10 + (s[4] - '0');
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
        return -1;
    }
    *minutes_out = hh * 60 + mm;
    return 0;
}

int util_format_hhmm(int minutes, char *buf, size_t sz) {
    int hh;
    int mm;
    int n;

    if (buf == NULL || sz < 6 || minutes < 0 || minutes >= 1440) {
        return -1;
    }
    hh = minutes / 60;
    mm = minutes % 60;
    n = snprintf(buf, sz, "%02d:%02d", hh, mm);
    return (n == 5) ? 0 : -1;
}
