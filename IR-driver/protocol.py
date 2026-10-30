"""Wire helpers for the DevTitans IR binary protocol."""
import struct
import zlib

MAGIC = b"IR"
VERSION = 1
HEADER_SIZE = 16
MAX_PAYLOAD = 1036
MAX_PACKET = HEADER_SIZE + MAX_PAYLOAD
CMD_READ = 0x01
CMD_WRITE = 0x02
CMD_PING = 0x03
RESPONSE_FLAG = 0x01
STATUS_OK = 0x00
STATUS_NO_DATA = 0x05


def crc32(data: bytes) -> int:
    """CRC-32/ISO-HDLC (zlib uses the reflected 0xEDB88320 form)."""
    return zlib.crc32(data) & 0xFFFFFFFF


def build_packet(command: int, sequence: int, payload: bytes = b"",
                 flags: int = 0, status: int = 0) -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload exceeds protocol maximum")
    header_without_crc = struct.pack(
        "<2sBBBBHI", MAGIC, VERSION, command, flags, status,
        sequence & 0xFFFF, len(payload))
    checksum = crc32(header_without_crc + payload)
    return header_without_crc + struct.pack("<I", checksum) + payload


def parse_header(header: bytes) -> dict:
    if len(header) != HEADER_SIZE:
        raise ValueError(f"header must be {HEADER_SIZE} bytes")
    magic, version, command, flags, status, sequence, length, checksum = \
        struct.unpack("<2sBBBBHII", header)
    return {"magic": magic, "version": version, "command": command,
            "flags": flags, "status": status, "sequence": sequence,
            "length": length, "crc": checksum}


def validate_packet(packet: bytes, *, command: int, sequence: int,
                    response: bool = True) -> dict:
    if len(packet) < HEADER_SIZE:
        raise ValueError(f"short packet: {len(packet)} bytes")
    fields = parse_header(packet[:HEADER_SIZE])
    if fields["magic"] != MAGIC:
        raise ValueError(f"bad magic: {fields['magic'].hex(' ')}")
    if fields["version"] != VERSION:
        raise ValueError(f"unsupported protocol version {fields['version']}")
    if fields["command"] != command:
        raise ValueError(f"command 0x{fields['command']:02x}, expected 0x{command:02x}")
    if bool(fields["flags"] & RESPONSE_FLAG) != response:
        raise ValueError(f"response flag is 0x{fields['flags']:02x}")
    if response and fields["status"] not in (STATUS_OK, STATUS_NO_DATA):
        raise ValueError(f"device returned status 0x{fields['status']:02x}")
    if fields["sequence"] != sequence:
        raise ValueError(f"sequence {fields['sequence']}, expected {sequence}")
    if fields["length"] > MAX_PAYLOAD:
        raise ValueError(f"payload length {fields['length']} exceeds maximum")
    expected_len = HEADER_SIZE + fields["length"]
    if len(packet) != expected_len:
        raise ValueError(f"packet length {len(packet)}, expected {expected_len}")
    actual_crc = crc32(packet[:12] + packet[16:])
    if fields["crc"] != actual_crc:
        raise ValueError(f"CRC 0x{fields['crc']:08x}, expected 0x{actual_crc:08x}")
    return fields


def parse_raw_payload(payload: bytes) -> dict:
    """Validate and decode the IR RAW payload envelope and symbol table."""
    if len(payload) < 12:
        raise ValueError(f"RAW payload too short: {len(payload)} bytes")
    symbol_count, flags, resolution_hz, carrier_hz = struct.unpack(
        "<HHII", payload[:12])
    expected_size = 12 + symbol_count * 4
    if symbol_count == 0:
        raise ValueError("RAW payload has no symbols")
    if len(payload) != expected_size:
        raise ValueError(
            f"RAW symbol_count={symbol_count} requires {expected_size} bytes, "
            f"got {len(payload)}")
    if resolution_hz == 0:
        raise ValueError("RAW resolution_hz must be nonzero")
    symbols = [struct.unpack_from("<HH", payload, 12 + i * 4)
               for i in range(symbol_count)]
    return {"symbol_count": symbol_count, "flags": flags,
            "resolution_hz": resolution_hz, "carrier_hz": carrier_hz,
            "symbols": symbols}
