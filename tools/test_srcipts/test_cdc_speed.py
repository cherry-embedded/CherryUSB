#
# Copyright (c) 2022, sakumisu
#
# SPDX-License-Identifier: Apache-2.0
#
import argparse
import time

import serial


DEFAULT_PORT = "COM17"
DEFAULT_BAUDRATE = 2000000
DEFAULT_LENGTH = 409600
MAX_CHUNK_LENGTH = 4 * 1024 * 1024
DEFAULT_REPORT_INTERVAL = 1.0
TEST_PATTERN = bytes([0xAA]) * MAX_CHUNK_LENGTH


def parse_size(value: str) -> int:
    text = value.strip().upper()
    units = {
        "B": 1,
        "K": 1024,
        "KB": 1024,
        "M": 1024 * 1024,
        "MB": 1024 * 1024,
        "G": 1024 * 1024 * 1024,
        "GB": 1024 * 1024 * 1024,
    }

    for unit, factor in sorted(units.items(), key=lambda item: len(item[0]), reverse=True):
        if text.endswith(unit):
            number = text[:-len(unit)].strip()
            return int(float(number) * factor)

    return int(float(text))


def format_speed(byte_count: int, duration: float) -> str:
    if duration <= 0:
        return "0.00 MB/s (0.00 Mbps)"

    mb_per_sec = byte_count / duration / 1024 / 1024
    mbps = byte_count * 8 / duration / 1000 / 1000
    return f"{mb_per_sec:.2f} MB/s ({mbps:.2f} Mbps)"


def print_progress(prefix: str, done_bytes: int, bytes_in_window: int, start_time: float, window_start: float) -> None:
    now = time.time()
    avg_speed = format_speed(done_bytes, now - start_time)
    instant_speed = format_speed(bytes_in_window, now - window_start)
    print(f"{prefix}: total {done_bytes} bytes, instant {instant_speed}, average {avg_speed}")


def run_tx(ser: serial.Serial, packet_size: int, report_interval: float) -> None:
    sent_bytes = 0
    start_time = time.time()
    window_start = start_time
    window_bytes = 0

    if packet_size <= len(TEST_PATTERN):
        payload = TEST_PATTERN[:packet_size]
    else:
        payload = bytes([0xAA]) * packet_size

    try:
        while True:
            sent = ser.write(payload)
            sent_bytes += sent
            window_bytes += sent

            now = time.time()
            if now - window_start >= report_interval:
                print_progress("TX", sent_bytes, window_bytes, start_time, window_start)
                window_start = now
                window_bytes = 0
    except KeyboardInterrupt:
        pass

    if window_bytes > 0:
        print_progress("TX", sent_bytes, window_bytes, start_time, window_start)

    total_time = time.time() - start_time
    print(f"TX stopped: {format_speed(sent_bytes, total_time)}")


def run_rx(ser: serial.Serial, packet_size: int, report_interval: float) -> None:
    recv_bytes = 0
    start_time = time.time()
    window_start = start_time
    window_bytes = 0

    try:
        while True:
            data = ser.read(packet_size)
            if not data:
                continue

            received = len(data)
            recv_bytes += received
            window_bytes += received

            now = time.time()
            if now - window_start >= report_interval:
                print_progress("RX", recv_bytes, window_bytes, start_time, window_start)
                window_start = now
                window_bytes = 0
    except KeyboardInterrupt:
        pass

    if window_bytes > 0:
        print_progress("RX", recv_bytes, window_bytes, start_time, window_start)

    total_time = time.time() - start_time
    print(f"RX stopped: {format_speed(recv_bytes, total_time)}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="CDC speed test tool")
    parser.add_argument("--port", default=DEFAULT_PORT, help=f"serial port, default: {DEFAULT_PORT}")
    parser.add_argument("--mode", choices=["tx", "rx"], required=True, help="tx: send data, rx: receive data")
    parser.add_argument("--chunk-length", type=parse_size, default=DEFAULT_LENGTH,
                        help=f"packet size per send/read, max 4M, default: {DEFAULT_LENGTH}, supports suffixes like 4K, 400K, 1M")
    parser.add_argument("--report-interval", type=float, default=DEFAULT_REPORT_INTERVAL,
                        help=f"speed report interval in seconds, default: {DEFAULT_REPORT_INTERVAL}")
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    print("Please remove usb log and change read & write buffer size then test speed")
    if args.chunk_length > MAX_CHUNK_LENGTH:
        raise ValueError("chunk length must be <= 4M")

    with serial.Serial(args.port, DEFAULT_BAUDRATE, timeout=None) as ser:
        if args.mode == "tx":
            print(f"Start TX test on {args.port}, packet_size={args.chunk_length} bytes")
            print("Press Ctrl+C to stop")
            ser.setDTR(0)
            run_tx(ser, args.chunk_length, args.report_interval)
        else:
            print(f"Start RX test on {args.port}, packet_size={args.chunk_length} bytes")
            print("Press Ctrl+C to stop")
            ser.setDTR(1)
            run_rx(ser, args.chunk_length, args.report_interval)


if __name__ == "__main__":
    main()
