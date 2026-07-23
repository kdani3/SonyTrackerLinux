#include "hidraw.h"
#include <stdio.h>
#include <math.h>

static double scale(int32_t raw, int32_t lmin, int32_t lmax,
                     int32_t pmin, int32_t pmax, int exponent) {
    if (lmax == lmin || (pmax == 0 && pmin == 0)) return (double)raw;
    double fraction = (double)(raw - lmin) / (double)(lmax - lmin);
    return (pmin + fraction * (pmax - pmin)) * pow(10.0, exponent);
}

static int16_t le16(const uint8_t *p) {
    return (int16_t)(p[0] | (p[1] << 8));
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s /dev/hidrawN\n", argv[0]);
        return 1;
    }

    int fd = hid_open(argv[1]);
    if (fd < 0) return 1;

    // Enable: reporting=all events, power=full, interval raw=0 (40ms) (Tested for XM5)
    uint8_t enable_report[2] = { 0x01, 0x03 };
    if (hid_set_feature(fd, enable_report, sizeof(enable_report)) < 0) {
        fprintf(stderr, "failed to enable sensor\n");
        return 1;
    }
    printf("sensor enabled, waiting for reports...\n");

    uint8_t report[64];
    for (int i = 0; i < 200; i++) {
        int n = hid_read_report_timeout(fd, report, sizeof(report), 2000);
        if (n < 0) break;
        if (n == 0) continue;
        if (n < 14 || report[0] != 0x01) continue; /* wrong report */

        int16_t rx = le16(&report[1]);
        int16_t ry = le16(&report[3]);
        int16_t rz = le16(&report[5]);
        int16_t gx = le16(&report[7]);
        int16_t gy = le16(&report[9]);
        int16_t gz = le16(&report[11]);
        uint8_t reset_counter = report[13];

        double rvx = scale(rx, -32767, 32767, -314159264, 314159265, -8);
        double rvy = scale(ry, -32767, 32767, -314159264, 314159265, -8);
        double rvz = scale(rz, -32767, 32767, -314159264, 314159265, -8);
        double gyx = scale(gx, -32767, 32767, -32, 32, 0);
        double gyy = scale(gy, -32767, 32767, -32, 32, 0);
        double gyz = scale(gz, -32767, 32767, -32, 32, 0);

        printf("raw: ");
        for (int b = 0; b < n; b++) printf("%02x ", report[b]);
            printf("\n");
        printf("rot=(%.4f %.4f %.4f) rad  gyro=(%.3f %.3f %.3f) rad/s  reset=%u\n",
               rvx, rvy, rvz, gyx, gyy, gyz, reset_counter);
    }

    hid_close(fd);
    return 0;
}