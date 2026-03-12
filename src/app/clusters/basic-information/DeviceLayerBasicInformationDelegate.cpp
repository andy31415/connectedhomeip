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
#include <lib/support/logging/CHIPLogging.h>
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
    case ManufacturingDate::Id: {
        constexpr size_t kMaxLen        = BasicInformation::Attributes::ManufacturingDate::TypeInfo::MaxLength();
        constexpr size_t kMaxDateLength = 8; // YYYYMMDD
        static_assert(kMaxLen > kMaxDateLength, "kMaxLen must be greater than kMaxDateLength");
        uint16_t manufacturingYear;
        uint8_t manufacturingMonth;
        uint8_t manufacturingDayOfMonth;
        size_t totalManufacturingDateLen = 0;

        CHIP_ERROR status = mContext.deviceInstanceInfoProvider.GetManufacturingDate(manufacturingYear, manufacturingMonth,
                                                                                     manufacturingDayOfMonth);

        if (status == CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND || status == CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE)
        {
            // Default to an empty string if not found
            buffer.reduce_size(0);
            return CHIP_NO_ERROR;
        }
        ReturnErrorOnFailure(status);

        if (buffer.size() < kMaxDateLength)
        {
            return CHIP_ERROR_BUFFER_TOO_SMALL;
        }

        // Format is YYYYMMDD
        int written =
            snprintf(buffer.data(), buffer.size(), "%04u%02u%02u", manufacturingYear, manufacturingMonth, manufacturingDayOfMonth);
        if (written < 0 || written >= static_cast<int>(buffer.size()))
        {
            return CHIP_ERROR_BUFFER_TOO_SMALL;
        }
        totalManufacturingDateLen = static_cast<size_t>(written);

        if (buffer.size() > kMaxDateLength)
        {
            MutableCharSpan vendorSuffixSpan(buffer.data() + kMaxDateLength, buffer.size() - kMaxDateLength);
            status = mContext.deviceInstanceInfoProvider.GetManufacturingDateSuffix(vendorSuffixSpan);
            if (status == CHIP_NO_ERROR)
            {
                totalManufacturingDateLen += vendorSuffixSpan.size();
            }
            // Suffix is optional, so errors other than NO_ERROR are ignored.
        }

        buffer.reduce_size(totalManufacturingDateLen);
        return CHIP_NO_ERROR;
    }
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

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetNumericAttribute(chip::AttributeId attributeId, uint32_t & value)
{
    using namespace Attributes;
    CHIP_ERROR err = CHIP_NO_ERROR;

    switch (attributeId)
    {
    case VendorID::Id: {
        uint16_t val;
        err = mContext.deviceInstanceInfoProvider.GetVendorId(val);
        if (err == CHIP_NO_ERROR)
            value = val;
        break;
    }
    case ProductID::Id: {
        uint16_t val;
        err = mContext.deviceInstanceInfoProvider.GetProductId(val);
        if (err == CHIP_NO_ERROR)
            value = val;
        break;
    }
    case HardwareVersion::Id: {
        uint16_t val;
        err = mContext.deviceInstanceInfoProvider.GetHardwareVersion(val);
        if (err == CHIP_NO_ERROR)
            value = val;
        break;
    }
    case SoftwareVersion::Id: {
        uint32_t val;
        err = mContext.configurationManager.GetSoftwareVersion(val);
        if (err == CHIP_NO_ERROR)
            value = val;
        break;
    }
    case LocalConfigDisabled::Id: {
        bool val;
        err = mContext.deviceInstanceInfoProvider.GetLocalConfigDisabled(val);
        if (err == CHIP_NO_ERROR)
            value = val;
        break;
    }
    case ConfigurationVersion::Id: {
        uint32_t val;
        err = mContext.configurationManager.GetConfigurationVersion(val);
        if (err == CHIP_NO_ERROR)
            value = val;
        break;
    }
    default:
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    return err;
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetLocalConfigDisabled(bool & localConfigDisabled)
{
    return mContext.deviceInstanceInfoProvider.GetLocalConfigDisabled(localConfigDisabled);
}

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetCapabilityMinima(Structs::CapabilityMinimaStruct::Type & outCapabilityMinima)
{
    constexpr uint16_t kMinCaseSessionsPerFabricMandatedBySpec = 3;

    auto capabilityMinimasFromDeviceInfo = mContext.deviceInstanceInfoProvider.GetSupportedCapabilityMinimaValues();

    outCapabilityMinima.caseSessionsPerFabric  = kMinCaseSessionsPerFabricMandatedBySpec;
    outCapabilityMinima.subscriptionsPerFabric = mContext.subscriptionsPerFabric;
    outCapabilityMinima.simultaneousInvocationsSupported =
        chip::MakeOptional<uint16_t>(capabilityMinimasFromDeviceInfo.simultaneousInvocationsSupported);
    outCapabilityMinima.simultaneousWritesSupported =
        chip::MakeOptional<uint16_t>(capabilityMinimasFromDeviceInfo.simultaneousWritesSupported);
    outCapabilityMinima.readPathsSupported = chip::MakeOptional<uint16_t>(capabilityMinimasFromDeviceInfo.readPathsSupported);
    outCapabilityMinima.subscribePathsSupported =
        chip::MakeOptional<uint16_t>(capabilityMinimasFromDeviceInfo.subscribePathsSupported);

    return CHIP_NO_ERROR;
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

CHIP_ERROR DeviceLayerBasicInformationDelegate::GetProductAppearance(Structs::ProductAppearanceStruct::Type & outProductAppearance)
{
    ProductFinishEnum finish;
    ReturnErrorOnFailure(mContext.deviceInstanceInfoProvider.GetProductFinish(&finish));
    outProductAppearance.finish = finish;

    ColorEnum color;
    CHIP_ERROR colorStatus = mContext.deviceInstanceInfoProvider.GetProductPrimaryColor(&color);

    if (colorStatus == CHIP_NO_ERROR)
    {
        outProductAppearance.primaryColor.SetNonNull(color);
    }
    else if (colorStatus == CHIP_ERROR_NOT_IMPLEMENTED)
    {
        outProductAppearance.primaryColor.SetNull();
    }
    else
    {
        return colorStatus;
    }

    return CHIP_NO_ERROR;
}

} // namespace BasicInformation
} // namespace Clusters
} // namespace app
} // namespace chip
