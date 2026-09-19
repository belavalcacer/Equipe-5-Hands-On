#!/usr/bin/env python3
"""Small UART smoke-test client; saved files contain only the IR wire payload."""
import argparse
from pathlib import Path
import struct
import time
import zlib

MAX_PAYLOAD = 1036
STATUS = ("OK", "INVALID_COMMAND", "INVALID_LENGTH", "INVALID_PAYLOAD", "CRC_ERROR",
          "NO_DATA", "BUSY", "TX_ERROR", "INTERNAL_ERROR")


def make_packet(command, sequence, payload=b""):
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload exceeds 1036 bytes")
    header = struct.pack("<2sBBBBHI", b"IR", 1, command, 0, 0, sequence, len(payload))
    return header + struct.pack("<I", zlib.crc32(header + payload)) + payload


def receive(port, command, sequence, timeout):
    deadline = time.monotonic() + timeout
    data = bytearray()
    while time.monotonic() < deadline:
        data.extend(port.read(1))
        while len(data) >= 2:
            if data[:2] != b"IR":
                del data[0]
                continue
            if len(data) < 16:
                break
            _, version, cmd, flags, status, seq, size, crc = struct.unpack("<2sBBBBHII", data[:16])
            if size > MAX_PAYLOAD:
                del data[0]
                continue
            if len(data) < 16 + size:
                break
            payload = bytes(data[16:16 + size])
            if zlib.crc32(data[:12] + payload) != crc:
                del data[0]
                continue
            del data[:16 + size]
            if (version, cmd, flags, seq) == (1, command, 1, sequence):
                return status, payload
    raise TimeoutError("response timeout; WRITE was not retried (it may already have executed)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="dedicated binary UART serial device")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=20)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("ping")
    for name in ("read", "write", "example"):
        commands.add_parser(name).add_argument("file", type=Path)
    args = parser.parse_args()
    if args.command == "example":
        args.file.write_bytes(struct.pack("<HHIIHH", 1, 0, 1000000, 0, 0x8CF8, 0x26D0))
        print(f"Saved MARK 3320 us / SPACE 9936 us to {args.file}")
        return
    if not args.port:
        parser.error("--port is required for ping/read/write")
    import serial  # Provided by the ESP-IDF Python environment (or pip install pyserial).
    command = {"read": 1, "write": 2, "ping": 3}[args.command]
    payload = args.file.read_bytes() if args.command == "write" else b""
    sequence = time.monotonic_ns() & 0xFFFF
    with serial.Serial(args.port, args.baud, timeout=0.1, write_timeout=2) as port:
        port.reset_input_buffer()
        port.write(make_packet(command, sequence, payload))
        port.flush()
        if args.command == "read":
            print("READ sent. Press the remote button now (firmware capture window: 5 s by default).", flush=True)
        status, response = receive(port, command, sequence, args.timeout)
    print(STATUS[status] if status < len(STATUS) else f"UNKNOWN_STATUS_{status}")
    if status:
        raise SystemExit(1)
    if args.command == "read":
        args.file.write_bytes(response)
        print(f"Saved {len(response)} bytes to {args.file}: {response.hex(' ')}")


if __name__ == "__main__":
    main()
