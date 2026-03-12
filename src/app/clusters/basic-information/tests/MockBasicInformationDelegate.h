/*
 *    Copyright (c) 2026 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
#pragma once

#include <app/clusters/basic-information/BasicInformationDelegate.h>
#include <clusters/BasicInformation/Enums.h>
#include <lib/support/CHIPMemString.h>
#include <lib/support/Span.h>
#include <platform/CHIPDeviceLayer.h>

namespace chip {
namespace app {
namespace Clusters {
namespace BasicInformation {
namespace tests {

using namespace chip::DeviceLayer;

static constexpr const char * kVendorName            = "TestVendor";
static constexpr const char * kProductName           = "TestProduct";
static constexpr const char * kHardwareVersionString = "HW1.0";
static constexpr const char * kPartNumber            = "PART123";
static constexpr const char * kProductURL            = "http://example.com";
static constexpr const char * kProductLabel          = "Label123";
static constexpr const char * kSerialNumber          = "SN123456";
static constexpr uint16_t kVendorId                  = static_cast<uint16_t>(VendorId::TestVendor1);
static constexpr uint16_t kProductId                 = 0x5678;
static constexpr uint16_t kHardwareVersion           = 1;
static constexpr uint16_t kManufacturingYear         = 2023;
static constexpr uint8_t kManufacturingMonth         = 6;
static constexpr uint8_t kManufacturingDay           = 15;
static constexpr ProductFinishEnum kProductFinish    = ProductFinishEnum::kMatte;
static constexpr ColorEnum kProductPrimaryColor      = ColorEnum::kBlack;
static constexpr size_t kCountryCodeLength = 2;
static constexpr const char * kUniqueId    = "TEST_UNIQUE_ID_12345";
static constexpr const char * kSoftwareVersionString = "SW1.0";
static constexpr uint32_t kSoftwareVersion         = 1;

// Helper function to safely copy strings and check for buffer size
inline CHIP_ERROR SafeCopyString(char * buf, size_t bufSize, const char * source)
{
    if (strlen(source) >= bufSize)
    {
        return CHIP_ERROR_BUFFER_TOO_SMALL;
    }
    Platform::CopyString(buf, bufSize, source);
    return CHIP_NO_ERROR;
}

class MockBasicInformationDelegate : public BasicInformationDelegate
{
public:
    CHIP_ERROR GetVendorName(char * buf, size_t bufSize) override { return SafeCopyString(buf, bufSize, kVendorName); }
    CHIP_ERROR GetProductName(char * buf, size_t bufSize) override { return SafeCopyString(buf, bufSize, kProductName); }
    CHIP_ERROR GetPartNumber(char * buf, size_t bufSize) override { return SafeCopyString(buf, bufSize, kPartNumber); }
    CHIP_ERROR GetProductURL(char * buf, size_t bufSize) override { return SafeCopyString(buf, bufSize, kProductURL); }
    CHIP_ERROR GetProductLabel(char * buf, size_t bufSize) override { return SafeCopyString(buf, bufSize, kProductLabel); }
    CHIP_ERROR GetSerialNumber(char * buf, size_t bufSize) override { return SafeCopyString(buf, bufSize, kSerialNumber); }
    CHIP_ERROR GetHardwareVersionString(char * buf, size_t bufSize) override
    {
        return SafeCopyString(buf, bufSize, kHardwareVersionString);
    }
    CHIP_ERROR GetSoftwareVersionString(char * buf, size_t bufSize) override
    {
        return SafeCopyString(buf, bufSize, kSoftwareVersionString);
    }
    CHIP_ERROR GetUniqueId(char * buf, size_t bufSize) override { return SafeCopyString(buf, bufSize, kUniqueId); }
    CHIP_ERROR GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day) override
    {
        year  = kManufacturingYear;
        month = kManufacturingMonth;
        day   = kManufacturingDay;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetManufacturingDateSuffix(MutableCharSpan & suffixBuffer) override
    {
        if (mManufacturingDateSuffix == nullptr)
        {
            suffixBuffer.reduce_size(0);
            return CHIP_NO_ERROR;
        }
        return CopyCharSpanToMutableCharSpan(CharSpan::fromCharString(mManufacturingDateSuffix), suffixBuffer);
    }

    CHIP_ERROR GetCountryCode(char * buf, size_t bufSize, size_t & countryCodeLen) override
    {
        VerifyOrReturnError(bufSize > kCountryCodeLength, CHIP_ERROR_BUFFER_TOO_SMALL);
        Platform::CopyString(buf, bufSize, mCountryCode);
        countryCodeLen = kCountryCodeLength;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GetVendorId(uint16_t & vendorId) override
    {
        vendorId = kVendorId;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetProductId(uint16_t & productId) override
    {
        productId = kProductId;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetHardwareVersion(uint16_t & hardwareVersion) override
    {
        hardwareVersion = kHardwareVersion;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetSoftwareVersion(uint32_t & softwareVersion) override
    {
        softwareVersion = kSoftwareVersion;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetProductFinish(ProductFinishEnum * finish) override
    {
        *finish = kProductFinish;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetProductPrimaryColor(ColorEnum * primaryColor) override
    {
        *primaryColor = kProductPrimaryColor;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetLocalConfigDisabled(bool & localConfigDisabled) override
    {
        localConfigDisabled = mLocalConfigDisabled;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GetConfigurationVersion(uint32_t & configurationVersion) override
    {
        configurationVersion = mConfigurationVersion;
        return CHIP_NO_ERROR;
    }
    DeviceInstanceInfoProvider::DeviceInfoCapabilityMinimas GetSupportedCapabilityMinimaValues() override
    {
        return {
            .simultaneousInvocationsSupported = CHIP_IM_MAX_NUM_COMMAND_HANDLER,
            .simultaneousWritesSupported      = CHIP_IM_MAX_NUM_WRITE_HANDLER,
            .readPathsSupported               = CHIP_IM_SERVER_MAX_NUM_PATH_GROUPS_FOR_READS,
            .subscribePathsSupported          = CHIP_IM_SERVER_MAX_NUM_PATH_GROUPS_FOR_SUBSCRIPTIONS,
        };
    }

    CHIP_ERROR SetLocalConfigDisabled(bool localConfigDisabled) override
    {
        mLocalConfigDisabled = localConfigDisabled;
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR StoreConfigurationVersion(uint32_t configurationVersion) override
    {
        mConfigurationVersion = configurationVersion;
        mStoreConfigVersionCalled++;
        return mStoreConfigVersionReturn;
    }
    CHIP_ERROR StoreCountryCode(const char * code, size_t codeLen) override
    {
        VerifyOrReturnError(codeLen == kCountryCodeLength, CHIP_ERROR_INVALID_ARGUMENT);
        Platform::CopyString(mCountryCode, sizeof(mCountryCode), code);
        return CHIP_NO_ERROR;
    }

    uint16_t GetSubscriptionsPerFabric() const override { return 3; }

    // Helper for tests
    void SetManufacturingDateSuffix(const char * suffix) { mManufacturingDateSuffix = suffix; }

public: // Public for test access
    uint32_t mConfigurationVersion = 10u;
    uint32_t mStoreConfigVersionCalled = 0u;
    CHIP_ERROR mStoreConfigVersionReturn = CHIP_NO_ERROR;

private:
    const char * mManufacturingDateSuffix = nullptr;
    char mCountryCode[kCountryCodeLength + 1] = "XX";
    bool mLocalConfigDisabled                 = false;
};

} // namespace tests
} // namespace BasicInformation
} // namespace Clusters
} // namespace app
} // namespace chip
