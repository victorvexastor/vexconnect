/* util.c — Logging and utility functions */

#include "vex.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

/* Hex encode bytes */
void vex_hex(const uint8_t *data, size_t len, char *out) {
    const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2]     = hex[data[i] >> 4];
        out[i * 2 + 1] = hex[data[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

/* Log with component tag */
void vex_log(const char *component, const char *fmt, ...) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm = localtime(&tv.tv_sec);

    fprintf(stderr, "[%02d:%02d:%02d] [%s] ",
            tm->tm_hour, tm->tm_min, tm->tm_sec, component);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

/* Current time in milliseconds */
uint64_t vex_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* randombytes — read from /dev/urandom */
void randombytes(unsigned char *x, unsigned long long xlen) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        /* Fallback: terrible but non-crashing */
        for (unsigned long long i = 0; i < xlen; i++)
            x[i] = (unsigned char)(time(NULL) ^ (i * 2654435761U));
        return;
    }
    read(fd, x, xlen);
    close(fd);
}
