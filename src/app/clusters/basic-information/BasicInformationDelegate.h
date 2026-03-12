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

#include <cstddef>
#include <cstdint>

#include <clusters/BasicInformation/Enums.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/Span.h>
#include <platform/DeviceInstanceInfoProvider.h>

namespace chip {
namespace app {
namespace Clusters {
namespace BasicInformation {

class BasicInformationDelegate
{
public:
    virtual ~BasicInformationDelegate() = default;

    // String Getters
    virtual CHIP_ERROR GetStringAttribute(chip::AttributeId attributeId, MutableCharSpan & buffer) = 0;

    // Value Getters
    virtual CHIP_ERROR GetVendorId(uint16_t & vendorId)                                                               = 0;
    virtual CHIP_ERROR GetProductId(uint16_t & productId)                                                             = 0;
    virtual CHIP_ERROR GetHardwareVersion(uint16_t & hardwareVersion)                                                 = 0;
    virtual CHIP_ERROR GetSoftwareVersion(uint32_t & softwareVersion)                                                 = 0;
    virtual CHIP_ERROR GetProductFinish(ProductFinishEnum * finish)                                                   = 0;
    virtual CHIP_ERROR GetProductPrimaryColor(ColorEnum * primaryColor)                                               = 0;
    virtual CHIP_ERROR GetLocalConfigDisabled(bool & localConfigDisabled)                                             = 0;
    virtual CHIP_ERROR GetConfigurationVersion(uint32_t & configurationVersion)                                       = 0;
    virtual DeviceLayer::DeviceInstanceInfoProvider::DeviceInfoCapabilityMinimas GetSupportedCapabilityMinimaValues() = 0;

    // Setters
    virtual CHIP_ERROR SetLocalConfigDisabled(bool localConfigDisabled)         = 0;
    virtual CHIP_ERROR StoreConfigurationVersion(uint32_t configurationVersion) = 0;
    virtual CHIP_ERROR StoreLocation(const CharSpan & code)                     = 0;

    // Misc
    virtual uint16_t GetSubscriptionsPerFabric() const = 0;
};

} // namespace BasicInformation
} // namespace Clusters
} // namespace app
} // namespace chip
