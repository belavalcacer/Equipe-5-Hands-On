#!/usr/bin/env python3
"""Binary IR client through /dev/devtitans_ir0; files contain only IR payloads."""
import argparse
import errno
import os
from pathlib import Path
import struct
import sys
import shlex
import time

from protocol import (
    CMD_PING,
    CMD_READ,
    CMD_WRITE,
    HEADER_SIZE,
    MAX_PAYLOAD,
    STATUS_NO_DATA,
    build_packet,
    crc32,
    parse_header,
    parse_raw_payload,
)

STATUS = (
    "OK",
    "INVALID_COMMAND",
    "INVALID_LENGTH",
    "INVALID_PAYLOAD",
    "CRC_ERROR",
    "NO_DATA",
    "BUSY",
    "TX_ERROR",
    "INTERNAL_ERROR",
)
READ_SIZE = 256
USB_READ_TIMEOUT = {errno.ETIMEDOUT, errno.EAGAIN}


def receive(port_fd, command, sequence, timeout):
    """Find and validate a response in the driver's FTDI-stripped byte stream."""
    deadline = time.monotonic() + timeout
    data = bytearray()

    while time.monotonic() < deadline:
        if len(data) < 2:
            needed = 2 - len(data)
        elif data[:2] != b"IR":
            del data[0]
            continue
        elif len(data) < HEADER_SIZE:
            needed = HEADER_SIZE - len(data)
        else:
            fields = parse_header(data[:HEADER_SIZE])
            if fields["length"] > MAX_PAYLOAD:
                del data[0]
                continue
            total_size = HEADER_SIZE + fields["length"]
            if len(data) >= total_size:
                packet = bytes(data[:total_size])
                actual_crc = crc32(packet[:12] + packet[HEADER_SIZE:])
                if actual_crc != fields["crc"]:
                    del data[0]
                    continue
                del data[:total_size]
                if (fields["magic"] == b"IR" and fields["version"] == 1
                        and fields["command"] == command
                        and fields["flags"] & 0x01
                        and fields["sequence"] == sequence):
                    return fields["status"], packet[HEADER_SIZE:]
                continue
            needed = total_size - len(data)

        try:
            chunk = os.read(port_fd, min(READ_SIZE, max(1, needed)))
        except OSError as exc:
            if exc.errno in USB_READ_TIMEOUT:
                continue
            raise
        if chunk:
            data.extend(chunk)

    raise TimeoutError("response timeout; WRITE was not retried (it may already have executed)")


def exchange(device, command, payload, timeout, after_send=None):
    sequence = time.monotonic_ns() & 0xFFFF
    packet = build_packet(command, sequence, payload)
    print(f"TX: {packet.hex(' ').upper()}")

    fd = os.open(device, os.O_RDWR | os.O_NOCTTY)
    try:
        sent = 0
        while sent < len(packet):
            count = os.write(fd, packet[sent:])
            if count <= 0:
                raise OSError("driver accepted no request bytes")
            sent += count
        if after_send:
            print(after_send, flush=True)
        status, response = receive(fd, command, sequence, timeout)
        return status, response
    finally:
        os.close(fd)


def status_name(status):
    return STATUS[status] if status < len(STATUS) else f"UNKNOWN_STATUS_{status}"


def make_example(path):
    # One mark of 3320 us followed by a space of 9936 us, at 1 MHz.
    payload = struct.pack("<HHIIHH", 1, 0, 1_000_000, 0, 0x8CF8, 0x26D0)
    path.write_bytes(payload)
    print(f"Saved MARK 3320 us / SPACE 9936 us to {path}")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="DevTitans IR protocol client through the custom Linux driver")
    parser.add_argument("--device", default="/dev/devtitans_ir0",
                        help="driver character device (default: /dev/devtitans_ir0)")
    parser.add_argument("--timeout", type=float, default=20,
                        help="response timeout in seconds (default: 20)")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("ping")
    commands.add_parser("read").add_argument(
        "file", type=Path, nargs="?", default=Path("ir_capture.bin"),
        help="output file for the IR payload (default: ir_capture.bin)")
    commands.add_parser("write").add_argument("file", type=Path,
                                                help="file containing only the IR payload")
    commands.add_parser("example").add_argument("file", type=Path,
                                                  help="create a small valid RAW payload")
    args = parser.parse_args(argv)

    if args.command == "example":
        make_example(args.file)
        return 0

    command = {"read": CMD_READ, "write": CMD_WRITE, "ping": CMD_PING}[args.command]
    payload = args.file.read_bytes() if args.command == "write" else b""
    if len(payload) > MAX_PAYLOAD:
        parser.error(f"IR payload exceeds {MAX_PAYLOAD} bytes")

    try:
        prompt = ("READ sent. Press the remote button now (firmware capture window)."
                  if args.command == "read" else None)
        status, response = exchange(args.device, command, payload, args.timeout,
                                    after_send=prompt)
        print(f"RX status: {status_name(status)}")
        if status != 0:
            if status == STATUS_NO_DATA:
                print("READ returned NO_DATA; no payload file was written")
            return 1
        if args.command == "read":
            details = parse_raw_payload(response)
            args.file.write_bytes(response)
            print(f"Saved {len(response)} IR payload bytes to {args.file}")
            print("RAW: symbols={symbol_count}, flags=0x{flags:04x}, "
                  "resolution={resolution_hz} Hz, carrier={carrier_hz} Hz".format(**details))
            print(f"Replay it with: sudo python3 tests/ir_cli.py write {shlex.quote(str(args.file))}")
        elif args.command == "write":
            print("WRITE accepted by device")
        else:
            print(f"PING response payload: {response.hex(' ') or '(empty)'}")
        print("RESULT: PASS")
        return 0
    except (OSError, TimeoutError, ValueError) as exc:
        print(f"RESULT: FAIL ({exc})")
        return 1


if __name__ == "__main__":
    sys.exit(main())
