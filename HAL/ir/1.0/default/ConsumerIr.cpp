#include "ConsumerIr.h"
#include <android-base/logging.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <cstdint>

namespace android::hardware::ir::V1_0::implementation {
namespace {
constexpr size_t kHeaderSize = 16;
constexpr size_t kPayloadPrefixSize = 12;
constexpr size_t kMaxSymbols = 256;

void put16(std::vector<uint8_t>* bytes, size_t offset, uint16_t value) {
    (*bytes)[offset] = static_cast<uint8_t>(value & 0xff);
    (*bytes)[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void put32(std::vector<uint8_t>* bytes, size_t offset, uint32_t value) {
    for (size_t index = 0; index < 4; ++index) (*bytes)[offset + index] = static_cast<uint8_t>(value >> (index * 8));
}

uint32_t crc32(const std::vector<uint8_t>& packet) {
    uint32_t crc = 0xffffffffU;
    auto append = [&crc](uint8_t value) {
        crc ^= value;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & static_cast<uint32_t>(-(crc & 1U)));
    };
    for (size_t index = 0; index < 12; ++index) append(packet[index]);
    for (size_t index = kHeaderSize; index < packet.size(); ++index) append(packet[index]);
    return ~crc;
}
} // namespace

ConsumerIr::ConsumerIr() {}

Return<bool> ConsumerIr::transmit(int32_t carrierFreq, const hidl_vec<int32_t>& pattern) {
    if (carrierFreq < 20000 || carrierFreq > 60000 || pattern.empty() || pattern.size() > kMaxSymbols * 2) {
        LOG(ERROR) << "HAL IR: padrão ou portadora inválidos.";
        return false;
    }

    const size_t symbols = (pattern.size() + 1) / 2;
    std::vector<uint8_t> packet(kHeaderSize + kPayloadPrefixSize + symbols * 4, 0);
    packet[0] = 'I'; packet[1] = 'R'; packet[2] = 1; packet[3] = 0x02; // WRITE
    put16(&packet, 6, 1); // sequence
    put32(&packet, 8, static_cast<uint32_t>(packet.size() - kHeaderSize));
    put16(&packet, kHeaderSize, static_cast<uint16_t>(symbols));
    put16(&packet, kHeaderSize + 2, 0);
    put32(&packet, kHeaderSize + 4, 1000000);
    put32(&packet, kHeaderSize + 8, static_cast<uint32_t>(carrierFreq));
    for (size_t index = 0; index < pattern.size(); ++index) {
        if (pattern[index] < 0 || pattern[index] > 32767 || (pattern[index] == 0 && index + 1 != pattern.size())) {
            LOG(ERROR) << "HAL IR: duração inválida no padrão.";
            return false;
        }
        put16(&packet, kHeaderSize + kPayloadPrefixSize + index * 2, static_cast<uint16_t>(pattern[index]));
    }
    put32(&packet, 12, crc32(packet));

    int carrierFd = open("/sys/class/devtitans_ir/devtitans_ir0/device/carrier", O_WRONLY);
    if (carrierFd >= 0) {
        std::string frequency = std::to_string(carrierFreq) + "\n";
        write(carrierFd, frequency.c_str(), frequency.length());
        close(carrierFd);
    }

    int fd = open("/dev/devtitans_ir0", O_WRONLY);
    if (fd < 0) {
        LOG(ERROR) << "HAL IR: não foi possível abrir /dev/devtitans_ir0.";
        return false;
    }
    ssize_t written = write(fd, packet.data(), packet.size());
    close(fd);
    return written == static_cast<ssize_t>(packet.size());
}

Return<void> ConsumerIr::getCarrierFreqs(getCarrierFreqs_cb callback) {
    std::vector<ConsumerIrFreqRange> ranges = {{.min = 38000, .max = 38000}};
    hidl_vec<ConsumerIrFreqRange> hidlRanges;
    hidlRanges.setToExternal(ranges.data(), ranges.size());
    callback(true, hidlRanges);
    return Void();
}

} // namespace android::hardware::ir::V1_0::implementation
