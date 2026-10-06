# IR firmware and protocol — quick guide

[Versão em português](ir_quickstart_pt.md) · [Full documentation (Portuguese)](ir_protocol.md)

## Introduction: from a remote button to RMT symbols

### IR, carrier, MARK, and SPACE

**IR** means infrared: the remote sends information using invisible light from
an LED. An IR protocol defines how timings represent bits, how address/command
fields are arranged, and how messages start, end, and repeat. The appliance must
recognize these rules to execute the command.

During transmission, the LED switches on and off rapidly. This modulation is
the **carrier**, normally 38 kHz in this project: about 38,000 cycles per second,
with a period of 26.3 µs. This is the LED switching frequency, not the optical
frequency of infrared light.

| Concept | Meaning in the signal |
|---|---|
| Carrier | Fast oscillation used while transmitting IR. |
| Duty cycle | Fraction of each cycle during which the LED is driven; currently 33%. |
| MARK | An interval containing the carrier: a burst of fast cycles. |
| SPACE | An interval without the carrier: a pause in transmission. |
| Envelope | The sequence of MARKs and SPACEs, ignoring the fast cycles inside each MARK. |

For example, a 9000 µs MARK at 38 kHz contains about 342 carrier cycles. It may
be followed by a 4500 µs SPACE. MARK/SPACE timings carry the information; carrier
frequency and duty cycle describe how light is emitted during each MARK.

A few carrier cycles look like this (schematic, not to scale):

```text
LED on          +---+        +---+        +---+
                |   |        |   |        |   |
LED off      ---+   +--------+   +--------+   +--------
                <----------->
                 one cycle: ~26.3 us (38 kHz)
                 on ~8.7 us; off ~17.6 us (33%)
time -------------------------------------------------->
```

Zooming out, each MARK contains many of these cycles. The drawings below share
the same MARK/SPACE boundaries, but are not to scale:

```text
                    MARK                SPACE          MARK
                <-------------><--------------------><------->
IR LED          |_|_|_|_|_|_|_|______________________|_|_|_|_

Envelope (1)    +-------------+                      +-------+
         (0) ---+             +----------------------+       +---

RX GPIO  (1) ---+             +----------------------+       +---
         (0)    +-------------+                      +-------+
time ----------------------------------------------------------->
```

The envelope indicates **carrier presence**, even while the LED briefly turns
off within each cycle. The demodulating receiver outputs the inverted envelope
on the GPIO; the firmware normalizes it back to MARK=1 and SPACE=0.

### NEC: an example IR protocol

Classic NEC starts with a 9 ms MARK and a 4.5 ms SPACE, followed by 32 bits:
address, inverted address, command, and inverted command, each 8 bits long.
Each byte is sent least significant bit first, and a final MARK ends the message.
Variants and specific repeat sequences also exist.

| NEC element | Nominal MARK | Nominal SPACE |
|---|---:|---:|
| Start | 9000 µs | 4500 µs |
| Bit 0 | 562.5 µs | 562.5 µs |
| Bit 1 | 562.5 µs | 1687.5 µs |

The pause duration distinguishes 0 from 1. **A data bit 0 also contains a MARK**;
MARK does not mean "data bit 1," and SPACE does not mean "data bit 0."
These timings and the frame structure are described in
[Espressif's NEC example](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/rmt.html#customize-rmt-encoder-for-nec-protocol).

NEC is used here to illustrate encoding. This firmware captures RAW timings
without interpreting addresses, commands, or NEC bits. A signal starting near
9 ms / 4.5 ms is not enough to identify its protocol, and the Gree capture should
not automatically be interpreted as NEC.

### What the receiver delivers to the ESP32

The receiver is **demodulating**: it detects the carrier and outputs the envelope
to the GPIO, without reproducing individual 38 kHz cycles. Its output is active-low:

| Optical state | Electrical RX GPIO level | Firmware-normalized level |
|---|---:|---:|
| MARK: carrier present | 0 | 1 |
| SPACE: carrier absent | 1 | 0 |

RMT therefore measures how long each low/high interval lasts. With this receiver,
it cannot measure the original carrier frequency; the payload uses `carrier_hz=0`.
During replay, TX recreates the carrier, using 38 kHz when that field is zero.

### How the envelope becomes a symbol table

RMT records two consecutive intervals per symbol:
`level0/duration0` and `level1/duration1`. At 1 MHz resolution, one tick equals
1 µs. The firmware removes leading idle and normalizes electrical levels to
MARK=1 and SPACE=0.

This illustrative excerpt contains an NEC start, a bit 0, a bit 1, and the final
MARK; the other data bits are omitted. Bit timings are rounded to 560 and 1690 µs
for readability. Actual captured timings vary.

| Symbol | Illustrative meaning | RX: level / duration 0 | RX: level / duration 1 | After normalization |
|---|---|---|---|---|
| 1 | Start | 0 / 9000 µs | 1 / 4500 µs | MARK 9000, SPACE 4500 |
| 2 | Bit 0 | 0 / 560 µs | 1 / 560 µs | MARK 560, SPACE 560 |
| 3 | Bit 1 | 0 / 560 µs | 1 / 1690 µs | MARK 560, SPACE 1690 |
| … | Remaining bits | … | … | … |
| Final | Ending MARK | 0 / 560 µs | Terminator, if no useful SPACE remains | MARK 560, SPACE 0 |

The final zero SPACE represents an absent final phase in the firmware format.
The electrical level of a zero-duration terminator carries no useful meaning.
A nonzero trailing SPACE may also be present depending on the hardware output.
An RMT symbol can contain a start sequence, a data bit, or another signal segment:
its unit is a pair of intervals with no built-in command meaning.

### Real example: a table of captured symbols

In the **70-symbol / 292-byte** capture provided during testing, the
`captura.ir` file starts with this RAW payload header:

```text
46 00 | 00 00 | 40 42 0F 00 | 00 00 00 00
  70    flags    1,000,000 Hz   unknown carrier
```

After these 12 bytes come the symbols, with 4 bytes per row. The table below
shows the first five and the last one, **already normalized by the firmware**:
`level0=1` represents MARK and `level1=0` represents SPACE. Resolution is
1 µs per tick, so duration values are also times in µs.

| Symbol (counting from 1) | Bytes in the file | `level0` | `duration0` (µs) | `level1` | `duration1` (µs) |
|---|---|---:|---:|---:|---:|
| 1 | `26 A3 6A 11` | 1 | 8998 | 0 | 4458 |
| 2 | `9C 82 66 06` | 1 | 668 | 0 | 1638 |
| 3 | `92 82 1B 02` | 1 | 658 | 0 | 539 |
| 4 | `96 82 1B 02` | 1 | 662 | 0 | 539 |
| 5 | `96 82 67 06` | 1 | 662 | 0 | 1639 |
| … | Symbols 6 through 69 omitted | … | … | … | … |
| 70 | `99 82 00 00` | 1 | 665 | 0 | 0 |

Reading the first row: IR was emitted for **8998 µs**, followed by a **4458 µs**
pause. Then the second row starts with a 668 µs MARK, and so on. At the active-low
receiver GPIO, these same useful intervals have inverted levels: MARK=0 and
SPACE=1. The table represents the normalized sequence, not a direct dump of
RX memory.

```text
Sequence: [MARK 8998][SPACE 4458][MARK 668][SPACE 1638] ... [MARK 665]
Symbols:  |----- symbol 1 ------|----- symbol 2 ------| ... |-- 70 --|
```

The last row has a zero-duration SPACE, indicating that there is no useful
second phase. The size adds up: `12 + 70 * 4 = 292 bytes`. This file contains
only the RAW payload; the UART transport header is not saved in it.
The 70 symbols do not necessarily mean 70 data bits: they also include start
intervals, pauses, and the ending.

### From the table to the UART packet

The firmware encodes each normalized phase as `(level << 15) | duration`.

**Why shift by 15?** Each phase occupies 16 bits: bit index 15 (the highest bit,
counting from zero) stores the level; the other 15 store the duration in ticks.
The `<< 15` operator moves the level into that position, and `|` combines the
two fields:

```text
bit:       15 | 14 ............ 0
         level| duration (15 bits)
         +----+-----------------+
MARK 560 | 1  | 000001000110000 | = 0x8230
         +----+-----------------+
          1 bit     15 bits

One RMT symbol = two phases = 32 bits (4 bytes):
         +----------------------+----------------------+
         | phase 0: level + time| phase 1: level + time|
         +----------------------+----------------------+
                  16 bits                16 bits
```

Example with level `1` (normalized MARK) and a duration of `560` ticks:

```c
(1 << 15) | 560
// 0x8000 | 0x0230 = 0x8230

// To extract the fields again:
level    = (value >> 15) & 1;
duration = value & 0x7FFF;
```

In the little-endian payload, `0x8230` becomes bytes `30 82`. The 15 bits allow
a maximum duration of `32767` ticks: **32.767 ms at 1 MHz**, per phase. This field
limit differs from the 40 ms idle threshold that ends a capture.

The first symbol in the illustrative NEC table (MARK 9000 + SPACE 4500) becomes:

```text
MARK 9000  → 0x8000 | 0x2328 = 0xA328 → bytes 28 A3
SPACE 4500 → 0x0000 | 0x1194 = 0x1194 → bytes 94 11
Symbol in the payload: 28 A3 94 11
```

These bytes are part of the RAW payload sent to Linux by READ. There are two
distinct protocol layers:

| Layer | What it defines |
|---|---|
| The remote's IR protocol, such as NEC | How timings represent bits and how those bits form appliance commands. |
| This project's UART protocol | How to transport RAW timings between Linux and ESP32 using READ/WRITE/PING, length, sequence ID, and CRC. |

WRITE reverses the path: bytes → MARK/SPACE phases → RMT TX → carrier on the IR
LED. The firmware can replay the timings without knowing which button they
represent. NEC bit order over the air and little-endian UART integers are
conventions belonging to different layers.

## What the firmware does

The ESP32 captures infrared pulse timings (RAW) **only when Linux sends READ**.
RX is disabled at startup. Each READ opens a new **5-second** window: the first
complete valid frame is returned, or `NO_DATA` if none completes before the
deadline. RX is disabled before replying. Previous captures are never reused,
and remote-control protocols are not decoded.

```text
Linux → READ → enable RX → wait up to 5 s → new frame or NO_DATA → Linux
Linux → WRITE → validation → RMT TX → IR transmitter
```

The RX callback notifies a queue. The `ir_rx` task normalizes levels and stops
reception after the first valid frame or timeout. Invalid frames are ignored
without restarting the deadline. The `ir_protocol` task reads UART bytes,
validates packets, executes commands, and replies. It waits for capture during
READ and transmission during WRITE. RX stays disabled during WRITE and PING.

## Files and initialization

| File or module | Responsibility |
|---|---|
| `main.c` | Starts the application. |
| `ir_rmt` | Creates and configures RMT channels. |
| `ir_rx` | Captures a fresh frame on demand, normalizes it, and enforces the timeout. |
| `ir_tx` | Transmits RAW timings using a copy encoder and carrier. |
| `ir_protocol` | Assembles/validates packets, calculates CRC, and handles commands. |
| `ir_transport_uart` | Transports bytes over UART. |
| `ir_types.h` | Defines IR phases, symbols, codes, and limits. |

`app_main()` runs this sequence:

```text
ir_rmt_init → ir_rx_init → ir_tx_init → ir_transport_init
           → ir_protocol_init
```

The `init` functions prepare resources; `ir_rx_init` creates the RX task and
`ir_protocol_init` creates the communication task. No `init` starts capture.
Then `app_main` returns while the tasks wait for requests. READ calls
`ir_rx_capture(code, timeout_ms)`; WRITE calls `ir_tx_send()`.

## Packets between Linux and firmware

```text
[Header: 16 bytes] [Payload: 0 to 1036 bytes]
```

All multibyte integers are **little-endian**. Fields appear in this order:

| Field | Bytes | Purpose |
|---|---:|---|
| `magic` | 2 | Bytes `49 52` (`IR`): identify a possible packet start. |
| `version` | 1 | Protocol version: `1`. |
| `command` | 1 | READ=`1`, WRITE=`2`, PING=`3`. |
| `flags` | 1 | Request=`0`, response=`1`. |
| `status` | 1 | Request=`0`; response indicates success or an error. |
| `sequence_id` | 2 | The firmware echoes this identifier in its reply. |
| `payload_length` | 4 | Payload size in bytes. |
| `crc32` | 4 | Detects data corruption. |

**CRC-32/ISO-HDLC** covers the first 12 header bytes followed by the payload,
excluding the CRC field itself. Python equivalent:
`zlib.crc32(header[:12] + payload)`. The parser searches for magic again after
losing synchronization and discards partial receptions after 250 ms without new bytes.

| Command | Linux → firmware | Firmware → Linux |
|---|---|---|
| READ (`0x01`) | No payload; starts a new capture. | First valid frame within the deadline, or `NO_DATA` (`0x05`) on timeout. |
| WRITE (`0x02`) | IR code as payload. | `OK` (`0x00`) after transmission, or an error; no payload. |
| PING (`0x03`) | No payload. | `OK`, no payload. |

Every READ requires a new capture. Send **one request at a time** and wait for
its reply. For READ, the serial timeout must exceed the capture window (for
example, 7 s for the default 5 s window). Allow at least 20 s for WRITE. The client
defaults to 20 s; increase `--timeout` if you extend the RX window. Retrying WRITE
may transmit the code again.
The remaining error codes are listed in the full documentation.

## IR payload contents

```text
symbol_count:u16 | flags:u16 | resolution_hz:u32 | carrier_hz:u32
symbols: symbol_count × 4 bytes
```

There are 1–256 symbols, flags=`0`, a 1 MHz resolution, and a total size of
`12 + symbol_count × 4` bytes. Each symbol contains two 16-bit phases:
MARK and SPACE. Bit 15 holds the level; the other 15 bits hold the duration in µs.
**MARK=1** means carrier present; **SPACE=0** means carrier absent.
RX already normalizes the active-low input: Linux does not invert any levels.

Durations range from 1 to 32767 µs; only the final SPACE may be zero.
Example: MARK 3320 µs + SPACE 9936 µs → `F8 8C D0 26`.
Captures use `carrier_hz=0` (unknown); TX uses 38 kHz in that case.
Accepted explicit carrier frequencies: 20–60 kHz.

Linux can reuse the **READ payload directly in WRITE**, building a new header
and recalculating its CRC.

## RMT symbols: from pulses to binary and hexadecimal

A `rmt_symbol_word_t` represents **two consecutive signal intervals**:
`level0/duration0` and `level1/duration1`. Each level uses 1 bit and each duration
uses 15 bits. At 1 MHz resolution, 1 tick = 1 µs. One symbol does not necessarily
represent one remote-control data bit: it describes levels and timings.

RX levels are electrical. Since the receiver is active-low, the firmware uses
`normalized_level = !rx_level`. For example:

| Interval | Electrical RX level | Duration | Normalized representation |
|---|---:|---:|---|
| First | 0 | 3320 ticks | MARK: level 1, 3320 µs |
| Second | 1 | 9936 ticks | SPACE: level 0, 9936 µs |

RMT may start with an idle SPACE; the firmware removes it and repacks the phases
so every protocol symbol starts with MARK followed by SPACE. The wire format is
serialized explicitly; the ESP-IDF structure is never sent directly.

To encode each normalized phase:

```text
phase = (level << 15) | duration

MARK:  0x8000 | 3320 = 0x8000 | 0x0CF8 = 0x8CF8
SPACE: 0x0000 | 9936 = 0x0000 | 0x26D0 = 0x26D0
```

| Phase | 16-bit binary (bit 15 = level) | Hexadecimal | Little-endian bytes |
|---|---|---|---|
| MARK | `1000 1100 1111 1000` | `8CF8` | `F8 8C` |
| SPACE | `0010 0110 1101 0000` | `26D0` | `D0 26` |

The complete symbol in the payload is **`F8 8C D0 26`**. In binary, these bytes are
`11111000 10001100 11010000 00100110`. Little-endian changes the byte order of each
integer; it does not reverse the bits within a byte.

To decode, reconstruct each 16-bit integer:

```text
phase = low_byte | (high_byte << 8)
level = (phase >> 15) & 1
duration = phase & 0x7FFF

F8 8C → 0x8CF8 → level 1, duration 3320 → MARK 3320 µs
D0 26 → 0x26D0 → level 0, duration 9936 → SPACE 9936 µs
```

For TX, normalized levels go directly to RMT: level 1 enables the carrier and
level 0 produces a pause. The 3320 µs MARK contains many 38 kHz carrier cycles;
it is not a single 38 kHz pulse. This conversion recovers RAW timings without
interpreting the data bits of a protocol such as NEC or Samsung.

## RX, TX, and PING examples

RX/TX are physical RMT operations. Their UART commands are READ to start a
capture and WRITE to transmit. Packets below use fixed sequence IDs for readability
and include valid CRCs; the Python client generates its own sequence ID and CRC.
The single-symbol frame is illustrative: actual captures usually contain multiple
symbols and may include a trailing pause.

### RX: capture and retrieve with READ

Run READ first. When the client reports that it sent the request, point the
remote at the GPIO18 receiver and press a button within 5 seconds. A command
sent before the request is not stored for later retrieval.

```sh
python tools/ir_client.py --port /dev/ttyUSB1 read capture.ir
```

Linux → firmware, READ with sequence 1:

```text
49 52 01 01 00 00 01 00 00 00 00 00 33 83 28 C5
```

Firmware → Linux, assuming a capture of MARK 3320 µs and SPACE 9936 µs:

```text
49 52 01 01 01 00 01 00 10 00 00 00 49 CE 6B ED
01 00 00 00 40 42 0F 00 00 00 00 00 F8 8C D0 26
```

The first line is the header; the second is the 16-byte payload saved to
`capture.ir`: one symbol, zero flags, 1 MHz resolution, and unknown carrier.
If no valid frame completes before the deadline, the reply is `NO_DATA` with no payload.

### TX: replay with WRITE

Send the saved payload or generate the single-symbol example:

```sh
python tools/ir_client.py --port /dev/ttyUSB1 write capture.ir
python tools/ir_client.py example example.ir
python tools/ir_client.py --port /dev/ttyUSB1 write example.ir
```

Linux → firmware, example WRITE with sequence 2:

```text
49 52 01 02 00 00 02 00 10 00 00 00 7D B0 DB 60
01 00 00 00 40 42 0F 00 00 00 00 00 F8 8C D0 26
```

The firmware validates the code, emits MARK for 3320 µs using a 38 kHz carrier,
then SPACE for 9936 µs on GPIO19. It replies `OK` with no payload when finished:

```text
49 52 01 02 01 00 02 00 00 00 00 00 C6 CD 9B B6
```

### PING: check communication

```sh
python tools/ir_client.py --port /dev/ttyUSB1 ping
```

Linux → firmware, PING with sequence 3:

```text
49 52 01 03 00 00 03 00 00 00 00 00 BE 0A 16 A6
```

Firmware → Linux, `OK` with the same sequence:

```text
49 52 01 03 01 00 03 00 00 00 00 00 20 0A BC 6A
```

PING neither captures nor transmits IR; the client prints `OK` upon receiving the reply.

## Defaults and quick test

RX uses **GPIO18**, TX uses **GPIO19**, with a **38 kHz / 33%** carrier and no DMA.
A capture ends after 40 ms at a constant level; longer gaps split frames. The
classic ESP32 idle threshold uses 16 bits; each RAW phase is still limited to
32,767 µs. Flash the firmware and capture again to test the new threshold;
an older file may contain only part of the command.
The binary UART is **UART1, TX GPIO25, RX GPIO26, 115200 8N1**.
UART0 is reserved for logs. In `idf.py menuconfig` → `RAW IR device`, configure
UART settings, `CONFIG_IR_RX_CAPTURE_TIMEOUT_MS` (default 5000 ms), and
`CONFIG_IR_RX_IDLE_US` (default 40000 µs). These are different timers: 5 s to obtain
a complete frame, and 40 ms at a constant level to delimit that frame. The parser's
250 ms timeout applies to incomplete UART bytes, not to IR capture.

With ESP-IDF 6.1 activated, build using `idf.py build`. To test with the
[serial client](../tools/ir_client.py), replace the port with the actual binary UART;
send READ, then press the remote button:

```sh
python tools/ir_client.py --port /dev/ttyUSB1 ping
python tools/ir_client.py --port /dev/ttyUSB1 read capture.ir
python tools/ir_client.py --port /dev/ttyUSB1 write capture.ir
```
