/*
 *    Copyright (c) 2026 Project CHIP Authors
 *    All rights reserved.
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
#include <platform/ConfigurationManager.h>
#include <platform/DeviceInstanceInfoProvider.h>
#include <platform/PlatformManager.h>

namespace chip {
namespace app {
namespace Clusters {
namespace BasicInformation {

class DeviceLayerBasicInformationDelegate : public BasicInformationDelegate
{
public:
    struct Context
    {
        DeviceLayer::DeviceInstanceInfoProvider & deviceInstanceInfoProvider;
        DeviceLayer::ConfigurationManager & configurationManager;
        DeviceLayer::PlatformManager & platformManager;
        uint16_t subscriptionsPerFabric;
    };

    DeviceLayerBasicInformationDelegate(Context ctx) : mContext(ctx) {}

    // BasicInformationDelegate Interface
    CHIP_ERROR GetVendorName(char * buf, size_t bufSize) override;
    CHIP_ERROR GetProductName(char * buf, size_t bufSize) override;
    CHIP_ERROR GetPartNumber(char * buf, size_t bufSize) override;
    CHIP_ERROR GetProductURL(char * buf, size_t bufSize) override;
    CHIP_ERROR GetProductLabel(char * buf, size_t bufSize) override;
    CHIP_ERROR GetSerialNumber(char * buf, size_t bufSize) override;
    CHIP_ERROR GetHardwareVersionString(char * buf, size_t bufSize) override;
    CHIP_ERROR GetSoftwareVersionString(char * buf, size_t bufSize) override;
    CHIP_ERROR GetUniqueId(char * buf, size_t bufSize) override;
    CHIP_ERROR GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day) override;
    CHIP_ERROR GetManufacturingDateSuffix(MutableCharSpan & suffixBuffer) override;
    CHIP_ERROR GetCountryCode(char * buf, size_t bufSize, size_t & codeLen) override;

    CHIP_ERROR GetVendorId(uint16_t & vendorId) override;
    CHIP_ERROR GetProductId(uint16_t & productId) override;
    CHIP_ERROR GetHardwareVersion(uint16_t & hardwareVersion) override;
    CHIP_ERROR GetSoftwareVersion(uint32_t & softwareVersion) override;
    CHIP_ERROR GetProductFinish(ProductFinishEnum * finish) override;
    CHIP_ERROR GetProductPrimaryColor(ColorEnum * primaryColor) override;
    CHIP_ERROR GetLocalConfigDisabled(bool & localConfigDisabled) override;
    CHIP_ERROR GetConfigurationVersion(uint32_t & configurationVersion) override;
    DeviceLayer::DeviceInstanceInfoProvider::DeviceInfoCapabilityMinimas GetSupportedCapabilityMinimaValues() override;

    CHIP_ERROR SetLocalConfigDisabled(bool localConfigDisabled) override;
    CHIP_ERROR StoreConfigurationVersion(uint32_t configurationVersion) override;
    CHIP_ERROR StoreCountryCode(const char * code, size_t codeLen) override;

    uint16_t GetSubscriptionsPerFabric() const override;

private:
    CHIP_ERROR IgnoreUnimplemented(CHIP_ERROR status, char * buf, size_t bufSize);

    Context mContext;
};

} // namespace BasicInformation
} // namespace Clusters
} // namespace app
} // namespace chip
