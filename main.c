#include "hidraw.h"
#include "quat.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static double scale(int32_t raw, int32_t lmin, int32_t lmax,
                     int32_t pmin, int32_t pmax, int exponent) {
    if (lmax == lmin || (pmax == 0 && pmin == 0)) return (double)raw;
    double fraction = (double)(raw - lmin) / (double)(lmax - lmin);
    return (pmin + fraction * (pmax - pmin)) * pow(10.0, exponent);
}

static int16_t le16(const uint8_t *p) {
    return (int16_t)(p[0] | (p[1] << 8));
}

static int udp_socket_to(const char *ip, int port, struct sockaddr_in *addr_out) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        fprintf(stderr, "socket() failed: %s\n", strerror(errno));
        return -1;
    }
    memset(addr_out, 0, sizeof(*addr_out));
    addr_out->sin_family = AF_INET;
    addr_out->sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr_out->sin_addr);
    return sock;
}

static void send_opentrack_pose(int sock, struct sockaddr_in *addr,
                                 double yaw, double pitch, double roll) {
    double pose[6] = { 0.0, 0.0, 0.0, yaw, pitch, roll };
    sendto(sock, pose, sizeof(pose), 0, (struct sockaddr*)addr, sizeof(*addr));
}

typedef struct {
    hid_device_entry device;
    int port;
    bool verbose;
} tracker_config;

static void *run_tracker(void *arg) {
    tracker_config *cfg = (tracker_config *)arg;

    int fd = hid_open(cfg->device.path);
    if (fd < 0) return NULL;

    uint8_t enable_report[2] = { 0x01, 0x03 };
    if (hid_set_feature(fd, enable_report, sizeof(enable_report)) < 0) {
        fprintf(stderr, "[%s] failed to enable sensor\n", cfg->device.name);
        hid_close(fd);
        return NULL;
    }

    struct sockaddr_in opentrack_addr;
    int udp_sock = udp_socket_to("127.0.0.1", cfg->port, &opentrack_addr);
    if (udp_sock < 0) { hid_close(fd); return NULL; }

    fprintf(stderr, "[%s] streaming to 127.0.0.1:%d\n", cfg->device.name, cfg->port);

    quat reference;
    bool have_reference = false;

    uint8_t report[64];
    for (;;) {
        int n = hid_read_report_timeout(fd, report, sizeof(report), 2000);
        if (n < 0) break;
        if (n == 0) continue;
        if (n < 14 || report[0] != 0x01) continue;

        int16_t rx = le16(&report[1]);
        int16_t ry = le16(&report[3]);
        int16_t rz = le16(&report[5]);

        vec3 rv;
        // Values taken from hid report descriptor tested for WH-1000XM5
        rv.x = scale(rx, -32767, 32767, -314159264, 314159265, -8);
        rv.y = scale(ry, -32767, 32767, -314159264, 314159265, -8);
        rv.z = scale(rz, -32767, 32767, -314159264, 314159265, -8);
        rv = remap_vec3(rv, AXIS_MAP_DEFAULT);

        quat current = rotvec_to_quat(rv);

        if (!have_reference) {
            reference = current;
            have_reference = true;
            if (cfg->verbose) fprintf(stderr, "[%s] recentered\n", cfg->device.name);
            continue;
        }

        quat relative = quat_relative_to_reference(current, reference);
        euler_deg e = quat_to_euler_deg(relative);

        send_opentrack_pose(udp_sock, &opentrack_addr, e.yaw, e.pitch, e.roll);

        if (cfg->verbose) {
            printf("[%s] yaw=%7.2f  pitch=%7.2f  roll=%7.2f\n",
                   cfg->device.name, e.yaw, e.pitch, e.roll);
        }
    }

    close(udp_sock);
    hid_close(fd);
    return NULL;
}

static void run_devices(const hid_device_entry *devices, int count, int base_port, bool verbose) {
    if (count == 1) {
        tracker_config cfg;
        cfg.device = devices[0];
        cfg.port = base_port;
        cfg.verbose = verbose;
        run_tracker(&cfg);
        return;
    }

    pthread_t threads[8];
    tracker_config configs[8];
    for (int i = 0; i < count; i++) {
        configs[i].device = devices[i];
        configs[i].port = base_port + i;
        configs[i].verbose = verbose;
        pthread_create(&threads[i], NULL, run_tracker, &configs[i]);
    }
    for (int i = 0; i < count; i++) {
        pthread_join(threads[i], NULL);
    }
}

static void print_help(const char *prog_name) {
    printf("Usage: %s [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  -d, --device PATH [PATH...]   Use specific hidraw device(s) directly,\n");
    printf("                                skipping autodetection\n");
    printf("  -p, --port PORT               Base UDP port (default: 4242)\n");
    printf("  -v, --verbose                 Print yaw/pitch/roll to stdout\n");
    printf("      --all                     Stream all detected headphones at once\n");
    printf("  -h, --help                    Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s\n", prog_name);
    printf("  %s -d /dev/hidraw3\n", prog_name);
    printf("  %s -d /dev/hidraw3 /dev/hidraw4\n", prog_name);
    printf("  %s --all --port 5000 --verbose\n", prog_name);
}

int main(int argc, char **argv) {
    hid_device_entry explicit_devices[8];
    int explicit_count = 0;
    int base_port = 4242;
    bool verbose = false;
    bool stream_all = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
            while (i + 1 < argc && argv[i + 1][0] != '-' && explicit_count < 8) {
                snprintf(explicit_devices[explicit_count].path,
                         sizeof(explicit_devices[0].path), "%s", argv[i + 1]);
                snprintf(explicit_devices[explicit_count].name,
                         sizeof(explicit_devices[0].name), "%s", argv[i + 1]);
                explicit_count++;
                i++;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: %s requires a value\n\n", argv[i]);
                print_help(argv[0]);
                return 1;
            }
            base_port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--all") == 0) {
            stream_all = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown option '%s'\n\n", argv[i]);
            print_help(argv[0]);
            return 1;
        } else {
            fprintf(stderr, "error: unexpected argument '%s'\n\n", argv[i]);
            print_help(argv[0]);
            return 1;
        }
    }

    // Explicit device(s) given: use exactly those, no scanning, no prompt
    if (explicit_count > 0) {
        run_devices(explicit_devices, explicit_count, base_port, verbose);
        return 0;
    }

    // No explicit device: scan for all matching headphones
    hid_device_entry entries[8];
    int count = hid_scan_all(entries, 8);

    if (count == 0) {
        fprintf(stderr, "no head-tracker device found (headset paired and connected?)\n");
        return 1;
    }

    if (count == 1) {
        run_devices(entries, 1, base_port, verbose);
        return 0;
    }

    // Multiple headphones found
    if (stream_all) {
        run_devices(entries, count, base_port, verbose);
        return 0;
    }

    // Multiple found, no --all: prompt. Accepts "2" or "1,3" for multiple
    printf("multiple headphones found:\n");
    for (int i = 0; i < count; i++) {
        printf("  [%d] %s (%s)\n", i + 1, entries[i].name, entries[i].path);
    }
    printf("select one or more, comma-separated (e.g. 1,3): ");
    fflush(stdout);

    char line[128];
    if (!fgets(line, sizeof(line), stdin)) {
        fprintf(stderr, "no input\n");
        return 1;
    }

    hid_device_entry chosen[8];
    int chosen_count = 0;
    char *token = strtok(line, ", \n");
    while (token && chosen_count < 8) {
        int idx = atoi(token);
        if (idx >= 1 && idx <= count) {
            chosen[chosen_count] = entries[idx - 1];
            chosen_count++;
        }
        token = strtok(NULL, ", \n");
    }

    if (chosen_count == 0) {
        fprintf(stderr, "invalid selection\n");
        return 1;
    }

    run_devices(chosen, chosen_count, base_port, verbose);
    return 0;
}