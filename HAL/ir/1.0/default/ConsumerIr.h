#ifndef ANDROID_HARDWARE_IR_V1_0_IR_H
#define ANDROID_HARDWARE_IR_V1_0_IR_H

#include <android/hardware/ir/1.0/IConsumerIr.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

namespace android {
namespace hardware {
namespace ir {
namespace V1_0 {
namespace implementation {

using ::android::hardware::ir::V1_0::ConsumerIrFreqRange;
using ::android::hardware::ir::V1_0::IConsumerIr;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

class ConsumerIr : public IConsumerIr {
public:
    // Construtor simples (sem depender do consumerir_device_t antigo)
    ConsumerIr();

    // Métodos obrigatórios da interface
    Return<bool> transmit(int32_t carrierFreq, const hidl_vec<int32_t>& pattern) override;
    Return<void> getCarrierFreqs(getCarrierFreqs_cb _hidl_cb) override;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace ir
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_IR_V1_0_IR_H