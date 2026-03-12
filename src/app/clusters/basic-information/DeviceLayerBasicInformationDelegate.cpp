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

#include <app/clusters/basic-information/DeviceLayerBasicInformationDelegate.h>
#include <clusters/BasicInformation/Enums.h>
#include <platform/CHIPDeviceError.h>

namespace chip {
namespace app {
namespace Clusters {
namespace BasicInformation {

using namespace DeviceLayer;

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetVendorName(char * buf, size_t bufSize)
{
    return mContext.deviceInstanceInfoProvider.GetVendorName(buf, bufSize);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetProductName(char * buf, size_t bufSize)
{
    return mContext.deviceInstanceInfoProvider.GetProductName(buf, bufSize);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetPartNumber(char * buf, size_t bufSize)
{
    return IgnoreUnimplemented(mContext.deviceInstanceInfoProvider.GetPartNumber(buf, bufSize), buf, bufSize);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetProductURL(char * buf, size_t bufSize)
{
    return IgnoreUnimplemented(mContext.deviceInstanceInfoProvider.GetProductURL(buf, bufSize), buf, bufSize);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetProductLabel(char * buf, size_t bufSize)
{
    return IgnoreUnimplemented(mContext.deviceInstanceInfoProvider.GetProductLabel(buf, bufSize), buf, bufSize);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetSerialNumber(char * buf, size_t bufSize)
{
    return IgnoreUnimplemented(mContext.deviceInstanceInfoProvider.GetSerialNumber(buf, bufSize), buf, bufSize);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetHardwareVersionString(char * buf, size_t bufSize)
{
    return mContext.deviceInstanceInfoProvider.GetHardwareVersionString(buf, bufSize);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetSoftwareVersionString(char * buf, size_t bufSize)
{
    return mContext.configurationManager.GetSoftwareVersionString(buf, bufSize);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetUniqueId(char * buf, size_t bufSize)
{
    return IgnoreUnimplemented(mContext.configurationManager.GetUniqueId(buf, bufSize), buf, bufSize);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day)
{
    return mContext.deviceInstanceInfoProvider.GetManufacturingDate(year, month, day);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetManufacturingDateSuffix(MutableCharSpan & suffixBuffer)
{
    return mContext.deviceInstanceInfoProvider.GetManufacturingDateSuffix(suffixBuffer);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetCountryCode(char * buf, size_t bufSize, size_t & codeLen)
{
    return mContext.configurationManager.GetCountryCode(buf, bufSize, codeLen);
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

CHIP_ERROR DeviceLayerBasicInformationDelegate::StoreCountryCode(const char * code, size_t codeLen)
{
    return mContext.configurationManager.StoreCountryCode(code, codeLen);
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
