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
#include <clusters/BasicInformation/Attributes.h>
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

static auto kVendorName                           = "TestVendor"_span;
static auto kProductName                          = "TestProduct"_span;
static auto kHardwareVersionString                = "HW1.0"_span;
static auto kPartNumber                           = "PART123"_span;
static auto kProductURL                           = "http://example.com"_span;
static auto kProductLabel                         = "Label123"_span;
static auto kSerialNumber                         = "SN123456"_span;
static constexpr uint16_t kVendorId               = static_cast<uint16_t>(VendorId::TestVendor1);
static constexpr uint16_t kProductId              = 0x5678;
static constexpr uint16_t kHardwareVersion        = 1;
static constexpr uint16_t kManufacturingYear      = 2023;
static constexpr uint8_t kManufacturingMonth      = 6;
static constexpr uint8_t kManufacturingDay        = 15;
static constexpr ProductFinishEnum kProductFinish = ProductFinishEnum::kMatte;
static constexpr ColorEnum kProductPrimaryColor   = ColorEnum::kBlack;
static constexpr size_t kLocationLength           = 2;
static auto kUniqueId                             = "TEST_UNIQUE_ID_12345"_span;
static auto kSoftwareVersionString                = "SW1.0"_span;
static constexpr uint32_t kSoftwareVersion        = 1;

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
    CHIP_ERROR GetStringAttribute(chip::AttributeId attributeId, MutableCharSpan & buffer) override
    {
        switch (attributeId)
        {
        case Attributes::VendorName::Id:
            return CopyCharSpanToMutableCharSpan(kVendorName, buffer);
        case Attributes::ProductName::Id:
            return CopyCharSpanToMutableCharSpan(kProductName, buffer);
        case Attributes::PartNumber::Id:
            return CopyCharSpanToMutableCharSpan(kPartNumber, buffer);
        case Attributes::ProductURL::Id:
            return CopyCharSpanToMutableCharSpan(kProductURL, buffer);
        case Attributes::ProductLabel::Id:
            return CopyCharSpanToMutableCharSpan(kProductLabel, buffer);
        case Attributes::SerialNumber::Id:
            return CopyCharSpanToMutableCharSpan(kSerialNumber, buffer);
        case Attributes::HardwareVersionString::Id:
            return CopyCharSpanToMutableCharSpan(kHardwareVersionString, buffer);
        case Attributes::SoftwareVersionString::Id:
            return CopyCharSpanToMutableCharSpan(kSoftwareVersionString, buffer);
        case Attributes::UniqueID::Id:
            return CopyCharSpanToMutableCharSpan(kUniqueId, buffer);
        case Attributes::Location::Id:
            return CopyCharSpanToMutableCharSpan(CharSpan::fromCharString(mLocation), buffer);
        default:
            return CHIP_ERROR_INVALID_ARGUMENT;
        }
    }

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

    CHIP_ERROR StoreLocation(const CharSpan & code) override
    {
        VerifyOrReturnError(code.size() == kLocationLength, CHIP_ERROR_INVALID_ARGUMENT);
        Platform::CopyString(mLocation, sizeof(mLocation), code.data());
        return CHIP_NO_ERROR;
    }

    uint16_t GetSubscriptionsPerFabric() const override { return 3; }
    // Helper for tests
    void SetManufacturingDateSuffix(const char * suffix) { mManufacturingDateSuffix = suffix; }

public: // Public for test access
    uint32_t mConfigurationVersion       = 10u;
    uint32_t mStoreConfigVersionCalled   = 0u;
    CHIP_ERROR mStoreConfigVersionReturn = CHIP_NO_ERROR;

private:
    const char * mManufacturingDateSuffix = nullptr;
    bool mLocalConfigDisabled             = false;
    char mLocation[kLocationLength + 1]   = "XX";
};

} // namespace tests
} // namespace BasicInformation
} // namespace Clusters
} // namespace app
} // namespace chip
