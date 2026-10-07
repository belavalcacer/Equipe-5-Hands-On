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

#pragma once

#include <aidl/devtitans/ir/BnIr.h>
#include <mutex>
#include <string>
#include <vector>

namespace aidl::devtitans::ir {

class Ir : public BnIr {
  public:
    Ir() = default;
    ~Ir() override = default;

    ::ndk::ScopedAStatus ping(bool* _aidl_return) override;
    ::ndk::ScopedAStatus getCarrier(int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus setCarrier(int32_t in_carrierHz, bool* _aidl_return) override;
    ::ndk::ScopedAStatus getCarrierFreqs(std::vector<int32_t>* _aidl_return) override;
    ::ndk::ScopedAStatus read(int32_t in_timeoutSeconds, std::vector<uint8_t>* _aidl_return) override;
    ::ndk::ScopedAStatus write(const std::vector<uint8_t>& in_payload, bool* _aidl_return) override;
    ::ndk::ScopedAStatus transmit(int32_t in_carrierFreqHz,
                                  const std::vector<int32_t>& in_pattern,
                                  bool* _aidl_return) override;

  private:
    static constexpr const char* kDevicePath = "/dev/devtitans_ir0";
    static constexpr const char* kSysfsCarrierPath =
        "/sys/class/devtitans_ir/devtitans_ir0/device/carrier";
    static constexpr const char* kSysfsCarrierAltPath =
        "/sys/class/devtitans_ir/devtitans_ir0/carrier";

    // Protocol constants from DevTitans IR protocol v1
    static constexpr uint16_t kMagic = 0x5249; // 'I' (0x49) | 'R' (0x52) << 8
    static constexpr uint8_t  kVersion = 1;
    static constexpr uint8_t  kCmdRead = 0x01;
    static constexpr uint8_t  kCmdWrite = 0x02;
    static constexpr uint8_t  kCmdPing = 0x03;
    static constexpr uint8_t  kFlagResponse = 0x01;

    static constexpr uint8_t  kStatusOk = 0x00;
    static constexpr uint8_t  kStatusInvalidCommand = 0x01;
    static constexpr uint8_t  kStatusInvalidLength = 0x02;
    static constexpr uint8_t  kStatusInvalidPayload = 0x03;
    static constexpr uint8_t  kStatusCrcError = 0x04;
    static constexpr uint8_t  kStatusNoData = 0x05;
    static constexpr uint8_t  kStatusBusy = 0x06;
    static constexpr uint8_t  kStatusTxError = 0x07;
    static constexpr uint8_t  kStatusInternalError = 0x08;

    static constexpr size_t   kHeaderSize = 16;
    static constexpr size_t   kMaxPayload = 1036;
    static constexpr size_t   kMaxPacket = kHeaderSize + kMaxPayload; // 1052 bytes
    static constexpr uint32_t kResolutionHz = 1000000;
    static constexpr uint32_t kDefaultCarrierHz = 38000;
    static constexpr uint32_t kMinCarrierHz = 20000;
    static constexpr uint32_t kMaxCarrierHz = 60000;
    static constexpr uint16_t kMaxSymbols = 256;

    std::mutex mLock;
    uint16_t mSequence{0};

    // Helper functions
    static uint32_t calculateCrc32(const uint8_t* data, size_t len, uint32_t initialCrc = 0xFFFFFFFFU);
    static uint32_t packetCrc32(const uint8_t* header, const uint8_t* payload, size_t len);

    bool exchangePacket(uint8_t command, const std::vector<uint8_t>& payload,
                        int timeoutSeconds, uint8_t& outStatus,
                        std::vector<uint8_t>& outResponsePayload);

    bool readSysfsCarrier(int32_t& carrierHz);
    bool writeSysfsCarrier(int32_t carrierHz);
};

} // namespace aidl::devtitans::ir
