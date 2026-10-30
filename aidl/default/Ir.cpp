/*
 * Copyright (C) 2024 DevTITANS
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Ir.h"

#include <android-base/logging.h>
#include <log/log.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <cstdlib>

namespace aidl::devtitans::ir {

uint32_t Ir::calculateCrc32(const uint8_t* data, size_t len, uint32_t initialCrc) {
    uint32_t crc = initialCrc;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (unsigned int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0);
        }
    }
    return crc;
}

uint32_t Ir::packetCrc32(const uint8_t* header, const uint8_t* payload, size_t len) {
    uint32_t crc = calculateCrc32(header, 12, 0xFFFFFFFFU);
    if (payload && len > 0) {
        crc = calculateCrc32(payload, len, crc);
    }
    return crc ^ 0xFFFFFFFFU;
}

bool Ir::readSysfsCarrier(int32_t& carrierHz) {
    std::string path = kSysfsCarrierPath;
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        path = kSysfsCarrierAltPath;
        fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    }
    if (fd < 0) {
        ALOGW("Carrier sysfs attribute not found, fallback to default %u Hz", kDefaultCarrierHz);
        carrierHz = kDefaultCarrierHz;
        return false;
    }
    char buf[32] = {0};
    ssize_t bytesRead = ::read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (bytesRead > 0) {
        carrierHz = std::atoi(buf);
        return true;
    }
    carrierHz = kDefaultCarrierHz;
    return false;
}

bool Ir::writeSysfsCarrier(int32_t carrierHz) {
    std::string path = kSysfsCarrierPath;
    int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        path = kSysfsCarrierAltPath;
        fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    }
    if (fd < 0) {
        ALOGW("Could not open carrier sysfs attribute for writing (%s)", path.c_str());
        return false;
    }
    std::string valStr = std::to_string(carrierHz) + "\n";
    ssize_t written = ::write(fd, valStr.c_str(), valStr.size());
    close(fd);
    return written == static_cast<ssize_t>(valStr.size());
}

bool Ir::exchangePacket(uint8_t command, const std::vector<uint8_t>& payload,
                        int timeoutSeconds, uint8_t& outStatus,
                        std::vector<uint8_t>& outResponsePayload) {
    if (payload.size() > kMaxPayload) {
        ALOGE("Payload size %zu exceeds maximum %zu", payload.size(), kMaxPayload);
        return false;
    }

    int fd = open(kDevicePath, O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd < 0) {
        ALOGE("Failed to open device %s: %s", kDevicePath, strerror(errno));
        return false;
    }

    uint16_t seq = ++mSequence;
    size_t packetSize = kHeaderSize + payload.size();
    std::vector<uint8_t> txPacket(packetSize, 0);

    // Header fields
    txPacket[0] = static_cast<uint8_t>(kMagic & 0xFF);         // 'I' (0x49)
    txPacket[1] = static_cast<uint8_t>((kMagic >> 8) & 0xFF);  // 'R' (0x52)
    txPacket[2] = kVersion;
    txPacket[3] = command;
    txPacket[4] = 0; // Request flags
    txPacket[5] = 0; // Request status
    txPacket[6] = static_cast<uint8_t>(seq & 0xFF);
    txPacket[7] = static_cast<uint8_t>((seq >> 8) & 0xFF);
    uint32_t payloadLen = static_cast<uint32_t>(payload.size());
    txPacket[8] = static_cast<uint8_t>(payloadLen & 0xFF);
    txPacket[9] = static_cast<uint8_t>((payloadLen >> 8) & 0xFF);
    txPacket[10] = static_cast<uint8_t>((payloadLen >> 16) & 0xFF);
    txPacket[11] = static_cast<uint8_t>((payloadLen >> 24) & 0xFF);

    if (!payload.empty()) {
        std::memcpy(txPacket.data() + kHeaderSize, payload.data(), payload.size());
    }

    uint32_t crc = packetCrc32(txPacket.data(), payload.empty() ? nullptr : payload.data(), payload.size());
    txPacket[12] = static_cast<uint8_t>(crc & 0xFF);
    txPacket[13] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    txPacket[14] = static_cast<uint8_t>((crc >> 16) & 0xFF);
    txPacket[15] = static_cast<uint8_t>((crc >> 24) & 0xFF);

    // Write complete packet to device
    size_t totalWritten = 0;
    while (totalWritten < packetSize) {
        ssize_t w = ::write(fd, txPacket.data() + totalWritten, packetSize - totalWritten);
        if (w < 0) {
            if (errno == EINTR) continue;
            ALOGE("Error writing to %s: %s", kDevicePath, strerror(errno));
            close(fd);
            return false;
        }
        if (w == 0) {
            ALOGE("Zero bytes written to %s", kDevicePath);
            close(fd);
            return false;
        }
        totalWritten += w;
    }

    // Read response stream
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
    std::vector<uint8_t> rxBuffer;
    rxBuffer.reserve(kMaxPacket);

    while (std::chrono::steady_clock::now() < deadline) {
        // Parse accumulated bytes
        while (rxBuffer.size() >= 2) {
            if (rxBuffer[0] != 0x49 || rxBuffer[1] != 0x52) {
                rxBuffer.erase(rxBuffer.begin());
                continue;
            }
            if (rxBuffer.size() < kHeaderSize) {
                break; // Need more bytes for header
            }

            uint8_t rxVersion = rxBuffer[2];
            uint8_t rxCommand = rxBuffer[3];
            uint8_t rxFlags = rxBuffer[4];
            uint8_t rxStatus = rxBuffer[5];
            uint16_t rxSeq = static_cast<uint16_t>(rxBuffer[6]) | (static_cast<uint16_t>(rxBuffer[7]) << 8);
            uint32_t rxPayloadLen = static_cast<uint32_t>(rxBuffer[8]) |
                                   (static_cast<uint32_t>(rxBuffer[9]) << 8) |
                                   (static_cast<uint32_t>(rxBuffer[10]) << 16) |
                                   (static_cast<uint32_t>(rxBuffer[11]) << 24);
            uint32_t rxCrc = static_cast<uint32_t>(rxBuffer[12]) |
                             (static_cast<uint32_t>(rxBuffer[13]) << 8) |
                             (static_cast<uint32_t>(rxBuffer[14]) << 16) |
                             (static_cast<uint32_t>(rxBuffer[15]) << 24);

            if (rxPayloadLen > kMaxPayload) {
                rxBuffer.erase(rxBuffer.begin());
                continue;
            }

            size_t expectedPacketSize = kHeaderSize + rxPayloadLen;
            if (rxBuffer.size() < expectedPacketSize) {
                break; // Need more bytes for full payload
            }

            uint32_t actualCrc = packetCrc32(rxBuffer.data(),
                                             rxPayloadLen > 0 ? rxBuffer.data() + kHeaderSize : nullptr,
                                             rxPayloadLen);
            if (actualCrc != rxCrc) {
                rxBuffer.erase(rxBuffer.begin());
                continue;
            }

            // Valid packet found
            if (rxVersion == kVersion && rxCommand == command &&
                (rxFlags & kFlagResponse) && rxSeq == seq) {
                outStatus = rxStatus;
                if (rxPayloadLen > 0) {
                    outResponsePayload.assign(rxBuffer.begin() + kHeaderSize,
                                              rxBuffer.begin() + expectedPacketSize);
                } else {
                    outResponsePayload.clear();
                }
                close(fd);
                return true;
            }

            // Unexpected packet, drop it and continue searching
            rxBuffer.erase(rxBuffer.begin(), rxBuffer.begin() + expectedPacketSize);
        }

        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) break;
        int remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        if (remainingMs <= 0) remainingMs = 1;

        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int pollRet = poll(&pfd, 1, remainingMs);
        if (pollRet < 0) {
            if (errno == EINTR) continue;
            ALOGE("poll() failed on %s: %s", kDevicePath, strerror(errno));
            break;
        }
        if (pollRet == 0) {
            continue;
        }

        if (pfd.revents & (POLLIN | POLLPRI)) {
            uint8_t chunk[256];
            ssize_t bytesRead = ::read(fd, chunk, sizeof(chunk));
            if (bytesRead > 0) {
                rxBuffer.insert(rxBuffer.end(), chunk, chunk + bytesRead);
            } else if (bytesRead < 0) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                ALOGE("read() failed on %s: %s", kDevicePath, strerror(errno));
                break;
            }
        } else if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            ALOGE("poll error event 0x%x on %s", pfd.revents, kDevicePath);
            break;
        }
    }

    close(fd);
    ALOGW("Timeout waiting for response to command 0x%02x (seq %u)", command, seq);
    return false;
}

::ndk::ScopedAStatus Ir::ping(bool* _aidl_return) {
    std::lock_guard<std::mutex> lock(mLock);
    uint8_t status = 0xFF;
    std::vector<uint8_t> resp;
    bool ok = exchangePacket(kCmdPing, {}, 2, status, resp);
    *_aidl_return = (ok && status == kStatusOk);
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus Ir::getCarrier(int32_t* _aidl_return) {
    std::lock_guard<std::mutex> lock(mLock);
    int32_t carrierHz = 0;
    readSysfsCarrier(carrierHz);
    *_aidl_return = carrierHz;
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus Ir::setCarrier(int32_t in_carrierHz, bool* _aidl_return) {
    std::lock_guard<std::mutex> lock(mLock);
    if (in_carrierHz < static_cast<int32_t>(kMinCarrierHz) ||
        in_carrierHz > static_cast<int32_t>(kMaxCarrierHz)) {
        ALOGE("Invalid carrier frequency %d Hz (supported: %u - %u Hz)",
              in_carrierHz, kMinCarrierHz, kMaxCarrierHz);
        *_aidl_return = false;
        return ::ndk::ScopedAStatus::ok();
    }
    *_aidl_return = writeSysfsCarrier(in_carrierHz);
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus Ir::getCarrierFreqs(std::vector<int32_t>* _aidl_return) {
    _aidl_return->clear();
    _aidl_return->push_back(kMinCarrierHz);
    _aidl_return->push_back(kMaxCarrierHz);
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus Ir::read(int32_t in_timeoutSeconds, std::vector<uint8_t>* _aidl_return) {
    std::lock_guard<std::mutex> lock(mLock);
    _aidl_return->clear();

    int timeout = (in_timeoutSeconds > 0) ? in_timeoutSeconds : 10;
    uint8_t status = 0xFF;
    std::vector<uint8_t> resp;

    bool ok = exchangePacket(kCmdRead, {}, timeout, status, resp);
    if (ok && status == kStatusOk) {
        *_aidl_return = std::move(resp);
    } else if (ok && status == kStatusNoData) {
        ALOGI("READ: No IR frame captured within the window");
    } else {
        ALOGW("READ failed (ok=%d, status=0x%02x)", ok, status);
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus Ir::write(const std::vector<uint8_t>& in_payload, bool* _aidl_return) {
    std::lock_guard<std::mutex> lock(mLock);
    if (in_payload.empty() || in_payload.size() > kMaxPayload) {
        ALOGE("Invalid payload size: %zu", in_payload.size());
        *_aidl_return = false;
        return ::ndk::ScopedAStatus::ok();
    }

    uint8_t status = 0xFF;
    std::vector<uint8_t> resp;
    bool ok = exchangePacket(kCmdWrite, in_payload, 5, status, resp);
    *_aidl_return = (ok && status == kStatusOk);
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus Ir::transmit(int32_t in_carrierFreqHz,
                                  const std::vector<int32_t>& in_pattern,
                                  bool* _aidl_return) {
    std::lock_guard<std::mutex> lock(mLock);
    if (in_pattern.empty()) {
        *_aidl_return = true;
        return ::ndk::ScopedAStatus::ok();
    }

    uint32_t carrierHz = (in_carrierFreqHz > 0) ? static_cast<uint32_t>(in_carrierFreqHz) : 0;
    if (carrierHz != 0 && (carrierHz < kMinCarrierHz || carrierHz > kMaxCarrierHz)) {
        ALOGE("Unsupported carrier frequency: %u Hz", carrierHz);
        *_aidl_return = false;
        return ::ndk::ScopedAStatus::ok();
    }

    size_t numIntervals = in_pattern.size();
    uint16_t symbolCount = static_cast<uint16_t>((numIntervals + 1) / 2);
    if (symbolCount == 0 || symbolCount > kMaxSymbols) {
        ALOGE("Invalid symbol count: %u (max: %u)", symbolCount, kMaxSymbols);
        *_aidl_return = false;
        return ::ndk::ScopedAStatus::ok();
    }

    size_t payloadSize = 12 + symbolCount * 4;
    std::vector<uint8_t> payload(payloadSize, 0);

    // symbol_count (uint16 LE)
    payload[0] = static_cast<uint8_t>(symbolCount & 0xFF);
    payload[1] = static_cast<uint8_t>((symbolCount >> 8) & 0xFF);
    // flags (uint16 LE) = 0
    payload[2] = 0;
    payload[3] = 0;
    // resolution_hz (uint32 LE) = 1,000,000
    payload[4] = static_cast<uint8_t>(kResolutionHz & 0xFF);
    payload[5] = static_cast<uint8_t>((kResolutionHz >> 8) & 0xFF);
    payload[6] = static_cast<uint8_t>((kResolutionHz >> 16) & 0xFF);
    payload[7] = static_cast<uint8_t>((kResolutionHz >> 24) & 0xFF);
    // carrier_hz (uint32 LE)
    payload[8] = static_cast<uint8_t>(carrierHz & 0xFF);
    payload[9] = static_cast<uint8_t>((carrierHz >> 8) & 0xFF);
    payload[10] = static_cast<uint8_t>((carrierHz >> 16) & 0xFF);
    payload[11] = static_cast<uint8_t>((carrierHz >> 24) & 0xFF);

    // Symbols encoding: level << 15 | duration
    for (uint16_t i = 0; i < symbolCount; ++i) {
        size_t idx0 = i * 2;
        size_t idx1 = i * 2 + 1;

        // Phase 0: MARK (level 1)
        uint16_t dur0 = static_cast<uint16_t>(in_pattern[idx0] > 32767 ? 32767 : in_pattern[idx0]);
        uint16_t phase0 = 0x8000U | (dur0 & 0x7FFFU);

        // Phase 1: SPACE (level 0)
        uint16_t dur1 = 0;
        if (idx1 < numIntervals) {
            dur1 = static_cast<uint16_t>(in_pattern[idx1] > 32767 ? 32767 : in_pattern[idx1]);
        }
        uint16_t phase1 = 0x0000U | (dur1 & 0x7FFFU);

        size_t offset = 12 + i * 4;
        payload[offset + 0] = static_cast<uint8_t>(phase0 & 0xFF);
        payload[offset + 1] = static_cast<uint8_t>((phase0 >> 8) & 0xFF);
        payload[offset + 2] = static_cast<uint8_t>(phase1 & 0xFF);
        payload[offset + 3] = static_cast<uint8_t>((phase1 >> 8) & 0xFF);
    }

    if (carrierHz > 0) {
        writeSysfsCarrier(carrierHz);
    }

    uint8_t status = 0xFF;
    std::vector<uint8_t> resp;
    bool ok = exchangePacket(kCmdWrite, payload, 10, status, resp);
    *_aidl_return = (ok && status == kStatusOk);
    return ::ndk::ScopedAStatus::ok();
}

} // namespace aidl::devtitans::ir
