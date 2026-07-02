/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>
#include <getopt.h>
#include "libusb.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define DEFAULT_VENDOR_ID   0xFFFF
#define DEFAULT_PRODUCT_ID  0xFFFF
#define DEFAULT_INTERFACE   0
#define DEFAULT_TIMEOUT     1000    /* 1 second */
#define MAX_PACKET_SIZE     (4 * 1024 * 1024)
#define REPORT_INTERVAL     1.0
#define TEST_PATTERN        0xAA

typedef struct {
    int vendor_id;
    int product_id;
    int interface_num;
    int timeout;
    char mode;              /* 't' for tx, 'r' for rx */
    uint32_t chunk_length;
    double report_interval;
    unsigned char endpoint_out;
    unsigned char endpoint_in;
} test_config_t;

static volatile int should_exit = 0;

void print_device_layout(libusb_device *device)
{
    struct libusb_config_descriptor *config_desc = NULL;
    int r = libusb_get_config_descriptor(device, 0, &config_desc);
    if (r < 0 || !config_desc) {
        return;
    }

    for (int if_num = 0; if_num < config_desc->bNumInterfaces; if_num++) {
        const struct libusb_interface *interface = &config_desc->interface[if_num];
        for (int alt = 0; alt < interface->num_altsetting; alt++) {
            const struct libusb_interface_descriptor *if_desc = &interface->altsetting[alt];
            printf("  Interface %d alt %d class 0x%02X endpoints %d\n",
                   if_desc->bInterfaceNumber,
                   if_desc->bAlternateSetting,
                   if_desc->bInterfaceClass,
                   if_desc->bNumEndpoints);

            for (int ep = 0; ep < if_desc->bNumEndpoints; ep++) {
                const struct libusb_endpoint_descriptor *ep_desc = &if_desc->endpoint[ep];
                const char *dir = (ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) ? "IN" : "OUT";
                const char *type = "OTHER";
                int transfer_type = ep_desc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK;
                if (transfer_type == LIBUSB_TRANSFER_TYPE_BULK) {
                    type = "BULK";
                } else if (transfer_type == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
                    type = "INT";
                } else if (transfer_type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) {
                    type = "ISO";
                } else if (transfer_type == LIBUSB_TRANSFER_TYPE_CONTROL) {
                    type = "CTRL";
                }

                printf("    EP 0x%02X %s %s\n", ep_desc->bEndpointAddress, dir, type);
            }
        }
    }

    libusb_free_config_descriptor(config_desc);
}

libusb_device_handle* open_device_with_diagnostics(libusb_context *ctx, int vendor_id, int product_id)
{
    libusb_device **list = NULL;
    ssize_t count = libusb_get_device_list(ctx, &list);
    if (count < 0) {
        fprintf(stderr, "Failed to enumerate USB devices: %s\n", libusb_error_name((int)count));
        return NULL;
    }

    libusb_device_handle *handle = NULL;
    int found_match = 0;

    for (ssize_t i = 0; i < count; i++) {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(list[i], &desc);
        if (r < 0) {
            continue;
        }

        if (desc.idVendor == vendor_id && desc.idProduct == product_id) {
            found_match = 1;

            printf("Matched device VID:0x%04X PID:0x%04X bus %u addr %u\n",
                   vendor_id,
                   product_id,
                   libusb_get_bus_number(list[i]),
                   libusb_get_device_address(list[i]));
            print_device_layout(list[i]);

            r = libusb_open(list[i], &handle);
            if (r == 0 && handle) {
                break;
            }

            fprintf(stderr,
                    "Matched VID:0x%04X PID:0x%04X but open failed: %s\n",
                    vendor_id, product_id, libusb_error_name(r));

#ifdef _WIN32
            if (r == LIBUSB_ERROR_NOT_FOUND || r == LIBUSB_ERROR_ACCESS) {
                fprintf(stderr,
                        "Hint: On Windows this usually means the target interface is not bound to WinUSB/libusbK.\n");
                fprintf(stderr,
                        "      Rebind the target interface driver (not the whole composite device) and retry --interface N.\n");
            }
#endif
        }
    }

    if (!handle && !found_match) {
        fprintf(stderr, "No USB device matched VID:0x%04X PID:0x%04X in enumeration\n",
                vendor_id, product_id);
    }

    libusb_free_device_list(list, 1);
    return handle;
}

void signal_handler(int signum)
{
    should_exit = 1;
}

double get_time_ms(void)
{
#ifdef _WIN32
    return (double)GetTickCount();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
#endif
}

const char* format_speed(uint64_t byte_count, double duration_sec)
{
    static char buffer[64];
    if (duration_sec <= 0) {
        snprintf(buffer, sizeof(buffer), "0.00 MB/s (0.00 Mbps)");
        return buffer;
    }

    double mb_per_sec = byte_count / duration_sec / 1024.0 / 1024.0;
    double mbps = byte_count * 8.0 / duration_sec / 1000.0 / 1000.0;
    snprintf(buffer, sizeof(buffer), "%.2f MB/s (%.2f Mbps)", mb_per_sec, mbps);
    return buffer;
}

void print_progress(const char *prefix, uint64_t done_bytes, uint64_t bytes_in_window,
                    double start_time, double window_start)
{
    double now = get_time_ms() / 1000.0;
    double avg_speed_duration = (now - start_time);
    double instant_speed_duration = (now - window_start);

    printf("%s: total %llu bytes, instant %s, average %s\n",
           prefix, (unsigned long long)done_bytes,
           format_speed(bytes_in_window, instant_speed_duration),
           format_speed(done_bytes, avg_speed_duration));
}

uint64_t parse_size(const char *value)
{
    char *endptr;
    double number = strtod(value, &endptr);

    if (endptr == value) {
        return 0;
    }

    while (*endptr && (*endptr == ' ' || *endptr == '\t')) {
        endptr++;
    }

    const char *unit = endptr;
    uint64_t factor = 1;

    if (strcasecmp(unit, "b") == 0 || strcmp(unit, "") == 0) {
        factor = 1;
    } else if (strcasecmp(unit, "k") == 0 || strcasecmp(unit, "kb") == 0) {
        factor = 1024;
    } else if (strcasecmp(unit, "m") == 0 || strcasecmp(unit, "mb") == 0) {
        factor = 1024 * 1024;
    } else if (strcasecmp(unit, "g") == 0 || strcasecmp(unit, "gb") == 0) {
        factor = 1024 * 1024 * 1024;
    }

    return (uint64_t)(number * factor);
}

int find_cdc_endpoints(libusb_device *device, test_config_t *config)
{
    struct libusb_device_descriptor desc;
    struct libusb_config_descriptor *config_desc;
    const struct libusb_interface *interface;
    const struct libusb_interface_descriptor *if_desc;
    const struct libusb_endpoint_descriptor *ep_desc;
    int r;

    r = libusb_get_device_descriptor(device, &desc);
    if (r < 0) {
        fprintf(stderr, "Failed to get device descriptor\n");
        return r;
    }

    r = libusb_get_config_descriptor(device, 0, &config_desc);
    if (r < 0) {
        fprintf(stderr, "Failed to get config descriptor\n");
        return r;
    }

    interface = &config_desc->interface[config->interface_num];
    if_desc = &interface->altsetting[0];

    for (int i = 0; i < if_desc->bNumEndpoints; i++) {
        ep_desc = &if_desc->endpoint[i];

        if ((ep_desc->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
            if (ep_desc->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) {
                config->endpoint_in = ep_desc->bEndpointAddress;
                printf("Found IN endpoint: 0x%02X\n", config->endpoint_in);
            } else {
                config->endpoint_out = ep_desc->bEndpointAddress;
                printf("Found OUT endpoint: 0x%02X\n", config->endpoint_out);
            }
        }
    }

    libusb_free_config_descriptor(config_desc);

    if (!config->endpoint_in || !config->endpoint_out) {
        fprintf(stderr, "Could not find IN or OUT endpoint\n");
        return LIBUSB_ERROR_NOT_FOUND;
    }

    return 0;
}

int run_tx(libusb_device_handle *handle, test_config_t *config)
{
    uint8_t *buffer = malloc(config->chunk_length);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        return LIBUSB_ERROR_NO_MEM;
    }

    memset(buffer, TEST_PATTERN, config->chunk_length);

    uint64_t sent_bytes = 0;
    double start_time = get_time_ms() / 1000.0;
    double window_start = start_time;
    uint64_t window_bytes = 0;

    printf("Starting TX test, packet size: %u bytes\n", config->chunk_length);
    printf("Press Ctrl+C to stop\n");

    while (!should_exit) {
        int transferred = 0;
        int r = libusb_bulk_transfer(handle, config->endpoint_out, buffer,
                                     config->chunk_length, &transferred, config->timeout);

        if (r < 0) {
            fprintf(stderr, "Transfer error: %s\n", libusb_error_name(r));
            continue;
        }

        sent_bytes += transferred;
        window_bytes += transferred;

        double now = get_time_ms() / 1000.0;
        if (now - window_start >= config->report_interval) {
            print_progress("TX", sent_bytes, window_bytes, start_time, window_start);
            window_start = now;
            window_bytes = 0;
        }
    }

    if (window_bytes > 0) {
        print_progress("TX", sent_bytes, window_bytes, start_time, window_start);
    }

    double total_time = get_time_ms() / 1000.0 - start_time;
    printf("TX stopped: %s\n", format_speed(sent_bytes, total_time));

    free(buffer);
    return 0;
}

int run_rx(libusb_device_handle *handle, test_config_t *config)
{
    uint8_t *buffer = malloc(config->chunk_length);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate buffer\n");
        return LIBUSB_ERROR_NO_MEM;
    }

    uint64_t recv_bytes = 0;
    double start_time = get_time_ms() / 1000.0;
    double window_start = start_time;
    uint64_t window_bytes = 0;

    printf("Starting RX test, packet size: %u bytes\n", config->chunk_length);
    printf("Press Ctrl+C to stop\n");

    while (!should_exit) {
        int transferred = 0;
        int r = libusb_bulk_transfer(handle, config->endpoint_in, buffer,
                                     config->chunk_length, &transferred, config->timeout);

        if (r < 0) {
            fprintf(stderr, "Transfer error: %s\n", libusb_error_name(r));
            continue;
        }

        if (transferred > 0) {
            recv_bytes += transferred;
            window_bytes += transferred;

            double now = get_time_ms() / 1000.0;
            if (now - window_start >= config->report_interval) {
                print_progress("RX", recv_bytes, window_bytes, start_time, window_start);
                window_start = now;
                window_bytes = 0;
            }
        }
    }

    if (window_bytes > 0) {
        print_progress("RX", recv_bytes, window_bytes, start_time, window_start);
    }

    double total_time = get_time_ms() / 1000.0 - start_time;
    printf("RX stopped: %s\n", format_speed(recv_bytes, total_time));

    free(buffer);
    return 0;
}

void print_usage(const char *prog_name)
{
    printf("Usage: %s [options]\n", prog_name);
    printf("Options:\n");
    printf("  -m, --mode <tx|rx>          Mode: tx (send) or rx (receive) [required]\n");
    printf("  -v, --vendor <id>           Vendor ID in hex (default: 0x%04X)\n", DEFAULT_VENDOR_ID);
    printf("  -p, --product <id>          Product ID in hex (default: 0x%04X)\n", DEFAULT_PRODUCT_ID);
    printf("  -i, --interface <num>       Interface number (default: %d)\n", DEFAULT_INTERFACE);
    printf("  -c, --chunk-length <size>   Packet size (default: 4096, supports K/M/G suffixes)\n");
    printf("  -r, --report-interval <sec> Report interval in seconds (default: %.1f)\n", REPORT_INTERVAL);
    printf("  -t, --timeout <ms>          USB timeout in milliseconds (default: %d)\n", DEFAULT_TIMEOUT);
    printf("  -h, --help                  Show this help message\n");
}

int main(int argc, char *argv[])
{
    test_config_t config = {
        .vendor_id = DEFAULT_VENDOR_ID,
        .product_id = DEFAULT_PRODUCT_ID,
        .interface_num = DEFAULT_INTERFACE,
        .timeout = DEFAULT_TIMEOUT,
        .mode = 0,
        .chunk_length = 4096,
        .report_interval = REPORT_INTERVAL,
        .endpoint_in = 0,
        .endpoint_out = 0,
    };

    static struct option long_options[] = {
        {"mode", required_argument, 0, 'm'},
        {"vendor", required_argument, 0, 'v'},
        {"product", required_argument, 0, 'p'},
        {"interface", required_argument, 0, 'i'},
        {"chunk-length", required_argument, 0, 'c'},
        {"report-interval", required_argument, 0, 'r'},
        {"timeout", required_argument, 0, 't'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "m:v:p:i:c:r:t:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'm':
                if (strcmp(optarg, "tx") == 0) {
                    config.mode = 't';
                } else if (strcmp(optarg, "rx") == 0) {
                    config.mode = 'r';
                } else {
                    fprintf(stderr, "Invalid mode: %s\n", optarg);
                    print_usage(argv[0]);
                    return 1;
                }
                break;
            case 'v':
                config.vendor_id = strtol(optarg, NULL, 16);
                break;
            case 'p':
                config.product_id = strtol(optarg, NULL, 16);
                break;
            case 'i':
                config.interface_num = atoi(optarg);
                break;
            case 'c':
                config.chunk_length = parse_size(optarg);
                if (config.chunk_length > MAX_PACKET_SIZE) {
                    fprintf(stderr, "Chunk length cannot exceed %u bytes\n", MAX_PACKET_SIZE);
                    return 1;
                }
                break;
            case 'r':
                config.report_interval = atof(optarg);
                break;
            case 't':
                config.timeout = atoi(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (!config.mode) {
        fprintf(stderr, "Error: mode (-m) is required\n");
        print_usage(argv[0]);
        return 1;
    }

    signal(SIGINT, signal_handler);

    libusb_context *ctx = NULL;
    int r = libusb_init(&ctx);
    if (r < 0) {
        fprintf(stderr, "Failed to initialize libusb: %s\n", libusb_error_name(r));
        return 1;
    }

    libusb_device_handle *handle = open_device_with_diagnostics(ctx,
                                                                config.vendor_id,
                                                                config.product_id);
    if (!handle) {
        fprintf(stderr, "Device not found (VID:0x%04X, PID:0x%04X), please use winusb device\n",
                config.vendor_id, config.product_id);
        libusb_exit(ctx);
        return 1;
    }

    printf("Device found: VID=0x%04X, PID=0x%04X\n", config.vendor_id, config.product_id);

    /* Find endpoints */
    libusb_device *device = libusb_get_device(handle);
    r = find_cdc_endpoints(device, &config);
    if (r < 0) {
        fprintf(stderr, "Failed to find CDC endpoints\n");
        libusb_close(handle);
        libusb_exit(ctx);
        return 1;
    }

    /* Claim interface */
    r = libusb_claim_interface(handle, config.interface_num);
    if (r < 0) {
        fprintf(stderr, "Failed to claim interface: %s\n", libusb_error_name(r));
        libusb_close(handle);
        libusb_exit(ctx);
        return 1;
    }

    printf("Interface claimed successfully\n");
    printf("Chunk length: %u bytes\n", config.chunk_length);

    int result = 0;
    if (config.mode == 't') {
        result = run_tx(handle, &config);
    } else {
        result = run_rx(handle, &config);
    }

    libusb_release_interface(handle, config.interface_num);
    libusb_close(handle);
    libusb_exit(ctx);

    return result;
}
