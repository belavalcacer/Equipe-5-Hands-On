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

#define LOG_TAG "devtitans_ir_aidl_hal_test"

#include <aidl/Gtest.h>
#include <aidl/Vintf.h>
#include <aidl/devtitans/ir/IIr.h>
#include <android-base/logging.h>
#include <android/binder_auto_utils.h>
#include <android/binder_manager.h>
#include <gtest/gtest.h>
#include <vector>

using ::aidl::devtitans::ir::IIr;
using ::ndk::SpAIBinder;

class DevtitansIrTest : public ::testing::TestWithParam<std::string> {
  public:
    virtual void SetUp() override {
        mIr = IIr::fromBinder(
                SpAIBinder(AServiceManager_waitForService(GetParam().c_str())));
        ASSERT_NE(mIr, nullptr);
    }

    std::shared_ptr<IIr> mIr;
};

// Test getCarrierFreqs() returns valid range [min, max]
TEST_P(DevtitansIrTest, GetCarrierFreqsTest) {
    std::vector<int32_t> freqs;
    const auto& ret = mIr->getCarrierFreqs(&freqs);
    ASSERT_TRUE(ret.isOk());
    ASSERT_EQ(freqs.size(), 2u);
    EXPECT_GE(freqs[0], 20000);
    EXPECT_LE(freqs[1], 60000);
    EXPECT_LE(freqs[0], freqs[1]);
}

// Test getCarrier() and setCarrier() with valid frequency
TEST_P(DevtitansIrTest, CarrierGetSetTest) {
    bool ok = false;
    auto ret = mIr->setCarrier(38000, &ok);
    ASSERT_TRUE(ret.isOk());

    int32_t carrier = 0;
    ret = mIr->getCarrier(&carrier);
    ASSERT_TRUE(ret.isOk());
}

// Test setCarrier() with invalid frequencies
TEST_P(DevtitansIrTest, BadCarrierTest) {
    bool ok = true;
    // Lower than min (20000)
    auto ret = mIr->setCarrier(10000, &ok);
    ASSERT_TRUE(ret.isOk());
    EXPECT_FALSE(ok);

    // Higher than max (60000)
    ret = mIr->setCarrier(100000, &ok);
    ASSERT_TRUE(ret.isOk());
    EXPECT_FALSE(ok);
}

// Test transmit() with valid pattern
TEST_P(DevtitansIrTest, TransmitTest) {
    bool ok = false;
    // Simple pattern of mark (9000 us) and space (4500 us)
    std::vector<int32_t> pattern = {9000, 4500, 560, 560};
    auto ret = mIr->transmit(38000, pattern, &ok);
    ASSERT_TRUE(ret.isOk());
}

// Test transmit() with bad carrier
TEST_P(DevtitansIrTest, TransmitBadCarrierTest) {
    bool ok = true;
    std::vector<int32_t> pattern = {1000, 1000};
    auto ret = mIr->transmit(1000, pattern, &ok);
    ASSERT_TRUE(ret.isOk());
    EXPECT_FALSE(ok);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DevtitansIrTest);
INSTANTIATE_TEST_SUITE_P(
        PerInstance, DevtitansIrTest,
        testing::ValuesIn(android::getAidlHalInstanceNames(IIr::descriptor)),
        ::android::PrintInstanceNameToString);
