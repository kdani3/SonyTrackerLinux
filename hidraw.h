#ifndef HIDRAW_H
#define HIDRAW_H

#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <poll.h>
#include <dirent.h>
#include <stdlib.h>

static int hid_open(const char *path) {
    int fd = open(path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "open(%s) failed: %s\n", path, strerror(errno));
        return -1;
    }
    return fd;
}

static void hid_close(int fd) {
    if (fd >= 0) close(fd);
}

static int hid_report_descriptor(int fd, uint8_t *out, size_t out_cap, int *out_len) {
    int size = 0;
    if (ioctl(fd, HIDIOCGRDESCSIZE, &size) < 0) return -1;
    if ((size_t)size > out_cap) return -1;

    struct hidraw_report_descriptor desc;
    memset(&desc, 0, sizeof(desc));
    desc.size = size;
    if (ioctl(fd, HIDIOCGRDESC, &desc) < 0) return -1;

    memcpy(out, desc.value, size);
    *out_len = size;
    return 0;
}

static int hid_raw_name(int fd, char *out, size_t out_len) {
    if (ioctl(fd, HIDIOCGRAWNAME(out_len), out) < 0) {
        out[0] = '\0';
        return -1;
    }
    return 0;
}

static int hid_read_report(int fd, uint8_t *buf, size_t buf_len) {
    ssize_t n = read(fd, buf, buf_len);
    if (n < 0) {
        fprintf(stderr, "read() failed: %s\n", strerror(errno));
        return -1;
    }
    return (int)n;
}

static int hid_read_report_timeout(int fd, uint8_t *buf, size_t buf_len, int timeout_ms) {
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int ready = poll(&pfd, 1, timeout_ms);
    if (ready == 0) {
        fprintf(stderr, "no report within %d ms\n", timeout_ms);
        return 0;
    }
    if (ready < 0) {
        fprintf(stderr, "poll() failed: %s\n", strerror(errno));
        return -1;
    }
    return hid_read_report(fd, buf, buf_len);
}

static int hid_set_feature(int fd, const uint8_t *buf, size_t len) {
    uint8_t tmp[32];
    if (len > sizeof(tmp)) return -1;
    memcpy(tmp, buf, len);
    if (ioctl(fd, HIDIOCSFEATURE(len), tmp) < 0) {
        fprintf(stderr, "HIDIOCSFEATURE failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/* ---- Autodetect helpers  ---- */

static int hid_descriptor_is_sensor_custom(const uint8_t *desc, int len) {
    if (len < 4) return 0;
    return desc[0] == 0x05 && desc[1] == 0x20 &&
           desc[2] == 0x09 && desc[3] == 0xe1;
}

static int hid_confirm_head_tracker(int fd) {
    uint8_t report[24];
    memset(report, 0, sizeof(report));
    report[0] = 0x02;

    if (ioctl(fd, HIDIOCGFEATURE(sizeof(report)), report) < 0) {
        return 0;
    }

    const char marker[] = "#AndroidHeadTracker#";
    size_t marker_len = sizeof(marker) - 1;
    if (sizeof(report) - 1 < marker_len) return 0;
    return memcmp(&report[1], marker, marker_len) == 0;
}


typedef struct {
    char path[32];
    char name[256];
} hid_device_entry;

/* Scans /dev/hidraw0..31, fills out[] with every matching head-tracker
   device found (path + Bluetooth name) returns how many were found. */
static int hid_scan_all(hid_device_entry *out, int max_entries) {
    int count = 0;
    for (int i = 0; i < 32 && count < max_entries; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/hidraw%d", i);

        int fd = open(path, O_RDWR);
        if (fd < 0) continue;

        uint8_t desc[4096];
        int desc_len = 0;
        if (hid_report_descriptor(fd, desc, sizeof(desc), &desc_len) == 0 &&
            hid_descriptor_is_sensor_custom(desc, desc_len) &&
            hid_confirm_head_tracker(fd)) {
            snprintf(out[count].path, sizeof(out[count].path), "%s", path);
            hid_raw_name(fd, out[count].name, sizeof(out[count].name));
            count++;
        }

        close(fd);
    }
    return count;
}



#endif