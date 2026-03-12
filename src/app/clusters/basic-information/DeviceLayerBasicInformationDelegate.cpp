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

#include "lib/core/CHIPError.h"
#include "lib/support/Span.h"
#include <app/clusters/basic-information/DeviceLayerBasicInformationDelegate.h>
#include <clusters/BasicInformation/Attributes.h>
#include <clusters/BasicInformation/Enums.h>
#include <platform/CHIPDeviceError.h>

namespace chip {
namespace app {
namespace Clusters {
namespace BasicInformation {

using namespace DeviceLayer;

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetStringAttribute(chip::AttributeId attributeId, MutableCharSpan & buffer)
{
    using namespace Attributes;

    CHIP_ERROR err = CHIP_NO_ERROR;
    size_t dataLen = 0;

    switch (attributeId)
    {
    case VendorName::Id:
        err = mContext.deviceInstanceInfoProvider.GetVendorName(buffer.data(), buffer.size());
        break;
    case ProductName::Id:
        err = mContext.deviceInstanceInfoProvider.GetProductName(buffer.data(), buffer.size());
        break;
    case PartNumber::Id:
        err = mContext.deviceInstanceInfoProvider.GetPartNumber(buffer.data(), buffer.size());
        break;
    case ProductURL::Id:
        err = mContext.deviceInstanceInfoProvider.GetProductURL(buffer.data(), buffer.size());
        break;
    case ProductLabel::Id:
        err = mContext.deviceInstanceInfoProvider.GetProductLabel(buffer.data(), buffer.size());
        break;
    case SerialNumber::Id:
        err = mContext.deviceInstanceInfoProvider.GetSerialNumber(buffer.data(), buffer.size());
        break;
    case HardwareVersionString::Id:
        err = mContext.deviceInstanceInfoProvider.GetHardwareVersionString(buffer.data(), buffer.size());
        break;
    case SoftwareVersionString::Id:
        err = mContext.configurationManager.GetSoftwareVersionString(buffer.data(), buffer.size());
        break;
    case UniqueID::Id:
        err = mContext.configurationManager.GetUniqueId(buffer.data(), buffer.size());
        break;
    case Location::Id: {
        constexpr size_t kExpectedFixedLocationLength = 2;
        err = mContext.configurationManager.GetCountryCode(buffer.data(), buffer.size(), dataLen);
        if ((err != CHIP_NO_ERROR) || (dataLen != kExpectedFixedLocationLength))
        {
            // Fallback that was here historically
            return CopyCharSpanToMutableCharSpan("XX"_span, buffer);
        }

        buffer.reduce_size(dataLen);
        return err;
    }
    default:
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    if (err == CHIP_NO_ERROR)
    {
        buffer.reduce_size(strlen(buffer.data()));
    }

    return IgnoreUnimplemented(err, buffer.data(), buffer.size());
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day)
{
    return mContext.deviceInstanceInfoProvider.GetManufacturingDate(year, month, day);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetManufacturingDateSuffix(MutableCharSpan & suffixBuffer)
{
    return mContext.deviceInstanceInfoProvider.GetManufacturingDateSuffix(suffixBuffer);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetVendorId(uint16_t & vendorId)
{
    return mContext.deviceInstanceInfoProvider.GetVendorId(vendorId);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetProductId(uint16_t & productId)
{
    return mContext.deviceInstanceInfoProvider.GetProductId(productId);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetHardwareVersion(uint16_t & hardwareVersion)
{
    return mContext.deviceInstanceInfoProvider.GetHardwareVersion(hardwareVersion);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetSoftwareVersion(uint32_t & softwareVersion)
{
    return mContext.configurationManager.GetSoftwareVersion(softwareVersion);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetProductFinish(ProductFinishEnum * finish)
{
    return mContext.deviceInstanceInfoProvider.GetProductFinish(finish);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetProductPrimaryColor(ColorEnum * primaryColor)
{
    return mContext.deviceInstanceInfoProvider.GetProductPrimaryColor(primaryColor);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetLocalConfigDisabled(bool & localConfigDisabled)
{
    return mContext.deviceInstanceInfoProvider.GetLocalConfigDisabled(localConfigDisabled);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetConfigurationVersion(uint32_t & configurationVersion)
{
    return mContext.configurationManager.GetConfigurationVersion(configurationVersion);
}

DeviceInstanceInfoProvider::DeviceInfoCapabilityMinimas DeviceLayerBasicInformationDelegate::GetSupportedCapabilityMinimaValues()
{
    return mContext.deviceInstanceInfoProvider.GetSupportedCapabilityMinimaValues();
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::SetLocalConfigDisabled(bool localConfigDisabled)
{
    return mContext.deviceInstanceInfoProvider.SetLocalConfigDisabled(localConfigDisabled);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::StoreConfigurationVersion(uint32_t configurationVersion)
{
    return mContext.configurationManager.StoreConfigurationVersion(configurationVersion);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::StoreLocation(const CharSpan & code)
{
    return mContext.configurationManager.StoreCountryCode(code.data(), code.size());
}

uint16_t DeviceLayerBasicInformationDelegate::GetSubscriptionsPerFabric() const
{
    return mContext.subscriptionsPerFabric;
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::IgnoreUnimplemented(CHIP_ERROR status, char * buf, size_t bufSize)
{
    if (status == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND || status == CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE)
    {
        if (bufSize > 0)
        {
            buf[0] = 0;
        }
        return CHIP_NO_ERROR;
    }
    return status;
}

} // namespace BasicInformation
} // namespace Clusters
} // namespace app
} // namespace chip
