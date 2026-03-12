/*
 *    Copyright (c) 2020-2025 Project CHIP Authors
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
#include <app/clusters/basic-information/BasicInformationCluster.h>
#include <app/clusters/basic-information/BasicInformationDelegate.h>

#include <app/SpecificationDefinedRevisions.h>
#include <app/persistence/AttributePersistence.h>
#include <app/persistence/AttributePersistenceProvider.h>
#include <app/persistence/PascalString.h>
#include <app/server-cluster/AttributeListBuilder.h>
#include <app/server-cluster/DefaultServerCluster.h>
#include <clusters/BasicInformation/Attributes.h>
#include <clusters/BasicInformation/Enums.h>
#include <clusters/BasicInformation/Events.h>
#include <clusters/BasicInformation/Metadata.h>
#include <clusters/BasicInformation/Structs.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/Span.h>
#include <platform/CHIPDeviceError.h>
#include <platform/ConfigurationManager.h>
#include <platform/PlatformManager.h>
#include <protocols/interaction_model/StatusCode.h>
#include <tracing/macros.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::BasicInformation;
using namespace chip::app::Clusters::BasicInformation::Attributes;
using namespace chip::DeviceLayer;

namespace {

// Unique ID became mandatory in 4. If we have no unique id, claim revision 3
inline constexpr uint32_t kRevisionWithoutUniqueId = 3;

// This is NOT the same as the auto-generated attributes:
// see comment below about UniqueID (which we make behave as optional)
//
// TLDR: DO NOT USE kMandatoryMetadata (because unique ID is special for backwards compat builds)
constexpr DataModel::AttributeEntry kMandatoryAttributes[] = {
    DataModelRevision::kMetadataEntry,
    VendorName::kMetadataEntry,
    VendorID::kMetadataEntry,
    ProductName::kMetadataEntry,
    ProductID::kMetadataEntry,
    NodeLabel::kMetadataEntry,
    Location::kMetadataEntry,
    HardwareVersion::kMetadataEntry,
    HardwareVersionString::kMetadataEntry,
    SoftwareVersion::kMetadataEntry,
    SoftwareVersionString::kMetadataEntry,
    CapabilityMinima::kMetadataEntry,
    SpecificationVersion::kMetadataEntry,
    MaxPathsPerInvoke::kMetadataEntry,
    ConfigurationVersion::kMetadataEntry,
    // NOTE: UniqueID used to NOT be mandatory in previous spec version, so we add
    // this as a separate condition
    // UniqueID::kMetadataEntry,

};

constexpr size_t kExpectedFixedLocationLength = 2;

constexpr size_t kMaxStringLength = std::max({
    DeviceLayer::ConfigurationManager::kMaxVendorNameLength,
    DeviceLayer::ConfigurationManager::kMaxHardwareVersionStringLength,
    DeviceLayer::ConfigurationManager::kMaxSoftwareVersionStringLength,
    DeviceLayer::ConfigurationManager::kMaxPartNumberLength,
    DeviceLayer::ConfigurationManager::kMaxProductURLLength,
    DeviceLayer::ConfigurationManager::kMaxProductLabelLength,
    DeviceLayer::ConfigurationManager::kMaxSerialNumberLength,
    DeviceLayer::ConfigurationManager::kMaxUniqueIDLength,
});

CHIP_ERROR ReadStringAttribute(BasicInformation::BasicInformationDelegate * delegate, AttributeId attributeId,
                               AttributeValueEncoder & encoder)
{
    char buffer[kMaxStringLength + 1];
    MutableCharSpan span(buffer, sizeof(buffer));
    ReturnErrorOnFailure(delegate->GetStringAttribute(attributeId, span));

    // explicit char span creation to not synthesize a writer for Span<char> on top of Span<const char>
    return encoder.Encode(CharSpan{ buffer, span.size() });
}

inline CHIP_ERROR ReadNumericAttribute(BasicInformation::BasicInformationDelegate * delegate, AttributeId attributeId,
                                       AttributeValueEncoder & aEncoder)
{
    uint32_t value = 0;
    ReturnErrorOnFailure(delegate->GetNumericAttribute(attributeId, value));
    return aEncoder.Encode(value);
}

inline CHIP_ERROR ReadLocalConfigDisabled(BasicInformation::BasicInformationDelegate * delegate, AttributeValueEncoder & aEncoder)
{
    bool localConfigDisabled = false;
    ReturnErrorOnFailure(delegate->GetLocalConfigDisabled(localConfigDisabled));
    return aEncoder.Encode(localConfigDisabled);
}

inline CHIP_ERROR ReadCapabilityMinima(AttributeValueEncoder & aEncoder, BasicInformation::BasicInformationDelegate * delegate)
{
    BasicInformation::Structs::CapabilityMinimaStruct::Type capabilityMinima;
    ReturnErrorOnFailure(delegate->GetCapabilityMinima(capabilityMinima));
    return aEncoder.Encode(capabilityMinima);
}

inline CHIP_ERROR ReadProductAppearance(BasicInformation::BasicInformationDelegate * delegate, AttributeValueEncoder & aEncoder)
{
    Structs::ProductAppearanceStruct::Type productAppearance;
    ReturnErrorOnFailure(delegate->GetProductAppearance(productAppearance));
    return aEncoder.Encode(productAppearance);
}

} // namespace

namespace chip::app::Clusters {

DataModel::ActionReturnStatus BasicInformationCluster::ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                                     AttributeValueEncoder & encoder)
{
    using namespace BasicInformation::Attributes;

    enum class AttributeReadType
    {
        kString,
        kNumeric,
        kOther
    };
    AttributeReadType readType    = AttributeReadType::kOther;
    const AttributeId attributeId = request.path.mAttributeId;

    switch (attributeId)
    {
    case FeatureMap::Id:
        return encoder.Encode<uint32_t>(0);
    case ClusterRevision::Id:
        if (!mEnabledOptionalAttributes.IsSet(UniqueID::Id))
        {
            return encoder.Encode(kRevisionWithoutUniqueId);
        }
        return encoder.Encode(BasicInformation::kRevision);
    case NodeLabel::Id:
        return encoder.Encode(mNodeLabel.Content());
    case LocalConfigDisabled::Id:
        return ReadLocalConfigDisabled(mDelegate, encoder);
    case DataModelRevision::Id:
        return encoder.Encode(Revision::kDataModelRevision);
    case CapabilityMinima::Id:
        return ReadCapabilityMinima(encoder, mDelegate);
    case ProductAppearance::Id:
        return ReadProductAppearance(mDelegate, encoder);
    case SpecificationVersion::Id:
        return encoder.Encode(Revision::kSpecificationVersion);
    case MaxPathsPerInvoke::Id:
        return encoder.Encode<uint16_t>(CHIP_CONFIG_MAX_PATHS_PER_INVOKE);
    case Reachable::Id:
        return encoder.Encode<bool>(true);

    // String Attributes
    case Location::Id:
    case VendorName::Id:
    case ProductName::Id:
    case HardwareVersionString::Id:
    case SoftwareVersionString::Id:
    case ManufacturingDate::Id:
    case PartNumber::Id:
    case ProductURL::Id:
    case ProductLabel::Id:
    case SerialNumber::Id:
    case UniqueID::Id:
        readType = AttributeReadType::kString;
        break;

    // Numeric Attributes
    case VendorID::Id:
    case ProductID::Id:
    case HardwareVersion::Id:
    case SoftwareVersion::Id:
    case ConfigurationVersion::Id:
        readType = AttributeReadType::kNumeric;
        break;

    default:
        return Protocols::InteractionModel::Status::UnsupportedAttribute;
    }

    if (readType == AttributeReadType::kString)
    {
        return ReadStringAttribute(mDelegate, attributeId, encoder);
    }

    if (readType == AttributeReadType::kNumeric)
    {
        return ReadNumericAttribute(mDelegate, attributeId, encoder);
    }

    // Should not be reached if kOther is not set for any fallthrough cases
    return Protocols::InteractionModel::Status::UnsupportedAttribute;
}

DataModel::ActionReturnStatus BasicInformationCluster::WriteAttribute(const DataModel::WriteAttributeRequest & request,
                                                                      AttributeValueDecoder & decoder)
{
    return NotifyAttributeChangedIfSuccess(request.path.mAttributeId, WriteImpl(request, decoder));
}

CHIP_ERROR BasicInformationCluster::IncreaseConfigurationVersion()
{
    uint32_t globalConfig = 0;
    ReturnErrorOnFailure(mDelegate->GetNumericAttribute(Attributes::ConfigurationVersion::Id, globalConfig));
    ReturnErrorOnFailure(mDelegate->StoreConfigurationVersion(globalConfig + 1));
    NotifyAttributeChanged(ConfigurationVersion::Id);
    return CHIP_NO_ERROR;
}

DataModel::ActionReturnStatus BasicInformationCluster::WriteImpl(const DataModel::WriteAttributeRequest & request,
                                                                 AttributeValueDecoder & decoder)
{
    using namespace BasicInformation::Attributes;

    AttributePersistence persistence(mContext->attributeStorage);

    switch (request.path.mAttributeId)
    {
    case Location::Id: {
        CharSpan location;
        ReturnErrorOnFailure(decoder.Decode(location));
        VerifyOrReturnError(location.size() == kExpectedFixedLocationLength, Protocols::InteractionModel::Status::ConstraintError);
        return mDelegate->StoreLocation(location);
    }
    case NodeLabel::Id: {
        CharSpan label;
        ReturnErrorOnFailure(decoder.Decode(label));
        VerifyOrReturnError(mNodeLabel.SetContent(label), Protocols::InteractionModel::Status::ConstraintError);
        return persistence.StoreString(request.path, mNodeLabel);
    }
    case LocalConfigDisabled::Id: {
        bool localConfigDisabled = false;
        ReturnErrorOnFailure(mDelegate->GetLocalConfigDisabled(localConfigDisabled));
        auto decodeStatus = persistence.DecodeAndStoreNativeEndianValue(request.path, decoder, localConfigDisabled);
        ReturnErrorOnFailure(mDelegate->SetLocalConfigDisabled(localConfigDisabled));
        return decodeStatus;
    }
    default:
        return Protocols::InteractionModel::Status::UnsupportedWrite;
    }
}

CHIP_ERROR BasicInformationCluster::Attributes(const ConcreteClusterPath & path,
                                               ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder)
{

    DataModel::AttributeEntry optionalAttributes[] = {
        ManufacturingDate::kMetadataEntry,   //
        PartNumber::kMetadataEntry,          //
        ProductURL::kMetadataEntry,          //
        ProductLabel::kMetadataEntry,        //
        SerialNumber::kMetadataEntry,        //
        LocalConfigDisabled::kMetadataEntry, //
        Reachable::kMetadataEntry,           //
        ProductAppearance::kMetadataEntry,   //

        // Optional because of forced multi-revision support for backwards compatibility
        // emulation: we emulate revision 3 when uniqueid is not enabled.
        UniqueID::kMetadataEntry, //
    };

    AttributeListBuilder listBuilder(builder);

    // NOTE: do NOT use kMandatoryMetadata (see comment on kMandatoryAttributes: UniqueID is special)
    return listBuilder.Append(Span(kMandatoryAttributes), Span(optionalAttributes), mEnabledOptionalAttributes);
}

CHIP_ERROR BasicInformationCluster::Startup(ServerClusterContext & context)
{
    ReturnErrorOnFailure(DefaultServerCluster::Startup(context));

    // Register this cluster as the PlatformManager delegate to receive shutdown events.
    //
    // We only register if the PlatformManager does not currently have a delegate.
    // This prevents the cluster from overwriting a delegate that may have been explicitly
    // registered by the application logic. If a delegate is already present, this cluster
    // will simply not intercept the shutdown signal via this mechanism.
    if (mPlatformManager.GetDelegate() == nullptr)
    {
        mPlatformManager.SetDelegate(this);
    }

    AttributePersistence persistence(context.attributeStorage);

    (void) persistence.LoadString({ kRootEndpointId, BasicInformation::Id, Attributes::NodeLabel::Id }, mNodeLabel);
    // Specialization because some platforms `#define` true/false as 1/0 and we get;
    // error: no matching function for call to
    //   'chip::app::AttributePersistence::LoadNativeEndianValue(<brace-enclosed initializer list>, bool&, int)'

    bool localConfigDisabled = false;
    (void) persistence.LoadNativeEndianValue<bool>({ kRootEndpointId, BasicInformation::Id, Attributes::LocalConfigDisabled::Id },
                                                   localConfigDisabled, false);

    // Propagate the restored 'LocalConfigDisabled' state to the DeviceInstanceInfoProvider
    // so the underlying platform layer is aware of the configuration.
    ReturnErrorOnFailure(mDelegate->SetLocalConfigDisabled(localConfigDisabled));

    return CHIP_NO_ERROR;
}

void BasicInformationCluster::Shutdown(ClusterShutdownType shutdownType)
{
    if (mPlatformManager.GetDelegate() == this)
    {
        mPlatformManager.SetDelegate(nullptr);
    }
    DefaultServerCluster::Shutdown(shutdownType);
}

void BasicInformationCluster::OnStartUp(uint32_t softwareVersion)
{
    // The StartUp event SHALL be emitted by a Node after completing a boot or reboot process
    VerifyOrReturn(mContext != nullptr);

    MATTER_TRACE_INSTANT("OnStartUp", "BasicInfo");
    ChipLogDetail(Zcl, "Emitting StartUp event");

    BasicInformation::Events::StartUp::Type event{ softwareVersion };

    DataModel::EventsGenerator & eventsGenerator = mContext->interactionContext.eventsGenerator;
    eventsGenerator.GenerateEvent(event, kRootEndpointId);
}

void BasicInformationCluster::OnShutDown()
{
    // The ShutDown event SHOULD be emitted on a best-effort basis by a Node prior to any orderly shutdown sequence.
    VerifyOrReturn(mContext != nullptr);

    MATTER_TRACE_INSTANT("OnShutDown", "BasicInfo");
    ChipLogDetail(Zcl, "Emitting ShutDown event");

    BasicInformation::Events::ShutDown::Type event;

    DataModel::EventsGenerator & eventsGenerator = mContext->interactionContext.eventsGenerator;
    eventsGenerator.GenerateEvent(event, kRootEndpointId);
    eventsGenerator.ScheduleUrgentEventDeliverySync();
}

} // namespace chip::app::Clusters
