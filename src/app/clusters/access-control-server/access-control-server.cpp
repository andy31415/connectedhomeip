/**
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#include <access/AccessControl.h>
#include <access/Privilege.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Commands.h>
#include <app/data-model-provider/ActionReturnStatus.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <app/server-cluster/DefaultServerCluster.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/TextOnlyLogging.h>
#include <optional>
#include <protocols/interaction_model/StatusCode.h>

#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
#include "ArlEncoder.h"
#include <access/AccessRestrictionProvider.h>
#endif

#include <app-common/zap-generated/cluster-objects.h>

#include <app/CommandHandler.h>
#include <app/ConcreteCommandPath.h>
#include <app/EventLogging.h>
#include <app/data-model/Encode.h>
#include <app/reporting/reporting.h>
#include <app/server-cluster/ServerClusterInterface.h>
#include <app/server-cluster/ServerClusterInterfaceRegistry.h>
#include <app/server/AclStorage.h>
#include <app/server/Server.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;
using namespace chip::Access;
using namespace chip::app::Clusters::Globals;

namespace AccessControlCluster = chip::app::Clusters::AccessControl;

using AclEvent       = Clusters::AccessControl::Events::AccessControlEntryChanged::Type;
using ChangeTypeEnum = Clusters::AccessControl::ChangeTypeEnum;
using Entry          = AccessControl::Entry;
using EntryListener  = AccessControl::EntryListener;
using ExtensionEvent = Clusters::AccessControl::Events::AccessControlExtensionChanged::Type;

#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
using ArlReviewEvent = Clusters::AccessControl::Events::FabricRestrictionReviewUpdate::Type;
#endif

// TODO(#13590): generated code doesn't automatically handle max length so do it manually
constexpr int kExtensionDataMaxLength = 128;

constexpr uint16_t kClusterRevision = 2;

namespace {

ActionReturnStatus ChipErrorToStatus(CHIP_ERROR err)
{
    // Map some common errors into an underlying IM error
    // Separate logging is done to not lose the original error location in case such
    // this are available.
    std::optional<ActionReturnStatus> mappedStatus;
    if (err == CHIP_ERROR_INVALID_ARGUMENT)
    {
        mappedStatus = Protocols::InteractionModel::Status::ConstraintError;
    }
    else if (err == CHIP_ERROR_NOT_FOUND)
    {
        // Not found is generally also illegal argument: caused a lookup into an invalid location,
        // like invalid subjects or targets.
        mappedStatus = Protocols::InteractionModel::Status::ConstraintError;
    }
    else if (err == CHIP_ERROR_NO_MEMORY)
    {
        mappedStatus = Protocols::InteractionModel::Status::ResourceExhausted;
    }

    if (mappedStatus.has_value())
    {
        ActionReturnStatus::StringStorage storage;
        ChipLogError(DataManagement, "Re-mapped %" CHIP_ERROR_FORMAT " into %s for IM return codes", err.Format(),
                     mappedStatus->c_str(storage));
        return *mappedStatus;
    }

    return err;
}

class AccessControlAttribute : public DefaultServerCluster,
                               public AccessControl::EntryListener
#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
    ,
                               public AccessRestrictionProvider::Listener
#endif
{
public:
    AccessControlAttribute() = default;

    ClusterId GetClusterId() const override { return AccessControlCluster::Id; }

    ActionReturnStatus ReadAttribute(const ReadAttributeRequest & request, AttributeValueEncoder & encoder) override;
    ActionReturnStatus WriteAttribute(const WriteAttributeRequest & request, AttributeValueDecoder & decoder) override;
    CHIP_ERROR Attributes(const ConcreteClusterPath & path, DataModel::ListBuilder<AttributeEntry> & builder) override;
    std::optional<ActionReturnStatus> InvokeCommand(const InvokeRequest & request, chip::TLV::TLVReader & input_arguments,
                                                    CommandHandler * handler) override;
    CHIP_ERROR AcceptedCommands(const ConcreteClusterPath & path, DataModel::ListBuilder<AcceptedCommandEntry> & builder) override;
    CHIP_ERROR GeneratedCommands(const ConcreteClusterPath & path, DataModel::ListBuilder<CommandId> & builder) override;

public:
    void OnEntryChanged(const SubjectDescriptor * subjectDescriptor, FabricIndex fabric, size_t index,
                        const AccessControl::Entry * entry, AccessControl::EntryListener::ChangeType changeType) override;

#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
    void MarkCommissioningRestrictionListChanged() override;

    void MarkRestrictionListChanged(FabricIndex fabricIndex) override;

    void OnFabricRestrictionReviewUpdate(FabricIndex fabricIndex, uint64_t token, Optional<CharSpan> instruction,
                                         Optional<CharSpan> arlRequestFlowUrl) override;
#endif

private:
    CHIP_ERROR ReadAcl(AttributeValueEncoder & aEncoder);
    CHIP_ERROR ReadExtension(AttributeValueEncoder & aEncoder);
    CHIP_ERROR WriteAcl(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder);
    CHIP_ERROR WriteExtension(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder);
#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
    CHIP_ERROR ReadCommissioningArl(AttributeValueEncoder & aEncoder);
    CHIP_ERROR ReadArl(AttributeValueEncoder & aEncoder);
#endif
} sAttribute;

CHIP_ERROR LogExtensionChangedEvent(const AccessControlCluster::Structs::AccessControlExtensionStruct::Type & item,
                                    const Access::SubjectDescriptor & subjectDescriptor,
                                    AccessControlCluster::ChangeTypeEnum changeType)
{
    ExtensionEvent event{ .changeType = changeType, .fabricIndex = subjectDescriptor.fabricIndex };

    if (subjectDescriptor.authMode == Access::AuthMode::kCase)
    {
        event.adminNodeID.SetNonNull(subjectDescriptor.subject);
    }
    else if (subjectDescriptor.authMode == Access::AuthMode::kPase)
    {
        event.adminPasscodeID.SetNonNull(PAKEKeyIdFromNodeId(subjectDescriptor.subject));
    }

    event.latestValue.SetNonNull(item);

    EventNumber eventNumber;
    CHIP_ERROR err = LogEvent(event, 0, eventNumber);
    if (CHIP_NO_ERROR != err)
    {
        ChipLogError(DataManagement, "AccessControlCluster: log event failed %" CHIP_ERROR_FORMAT, err.Format());
    }

    return err;
}

CHIP_ERROR CheckExtensionEntryDataFormat(const ByteSpan & data)
{
    CHIP_ERROR err;

    TLV::TLVReader reader;
    reader.Init(data);

    auto containerType = TLV::kTLVType_List;
    err                = reader.Next(containerType, TLV::AnonymousTag());
    VerifyOrReturnError(err == CHIP_NO_ERROR, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    err = reader.EnterContainer(containerType);
    VerifyOrReturnError(err == CHIP_NO_ERROR, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    while ((err = reader.Next()) == CHIP_NO_ERROR)
    {
        VerifyOrReturnError(TLV::IsProfileTag(reader.GetTag()), CHIP_IM_GLOBAL_STATUS(ConstraintError));
    }
    VerifyOrReturnError(err == CHIP_END_OF_TLV, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    err = reader.ExitContainer(containerType);
    VerifyOrReturnError(err == CHIP_NO_ERROR, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    err = reader.Next();
    VerifyOrReturnError(err == CHIP_END_OF_TLV, CHIP_IM_GLOBAL_STATUS(ConstraintError));

    return CHIP_NO_ERROR;
}

CHIP_ERROR AccessControlAttribute::Attributes(const ConcreteClusterPath & path, DataModel::ListBuilder<AttributeEntry> & builder)
{

    static constexpr std::array<AttributeEntry, 5> kAttributes{ {
        {
            AccessControlCluster::Attributes::Acl::Id,
            BitFlags<AttributeQualityFlags>{ AttributeQualityFlags::kListAttribute },
            Access::Privilege::kAdminister,
            Access::Privilege::kAdminister,
        },
        {
            AccessControlCluster::Attributes::SubjectsPerAccessControlEntry::Id,
            {},
            Access::Privilege::kView,
        },
        {
            AccessControlCluster::Attributes::TargetsPerAccessControlEntry::Id,
            {},
            Access::Privilege::kView,
        },
        {
            AccessControlCluster::Attributes::AccessControlEntriesPerFabric::Id,
            {},
            Access::Privilege::kView,
        },
    } };

    ReturnErrorOnFailure(builder.ReferenceExisting(kAttributes));

    /// TODO: this should be IF AND ONLY IF extension attributes exist
    ReturnErrorOnFailure(builder.EnsureAppendCapacity(1));
    ReturnErrorOnFailure(builder.Append({
        AccessControlCluster::Attributes::Extension::Id,
        BitFlags<AttributeQualityFlags>{ AttributeQualityFlags::kListAttribute },
        Access::Privilege::kAdminister,
        Access::Privilege::kAdminister,
    }));

#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
    ReturnErrorOnFailure(builder.EnsureAppendCapacity(2));
    ReturnErrorOnFailure(builder.Append({
        AccessControlCluster::Attributes::CommissioningARL::Id,
        BitFlags<AttributeQualityFlags>{ AttributeQualityFlags::kListAttribute },
        Access::Privilege::kView,
    }));
    ReturnErrorOnFailure(builder.Append({
        AccessControlCluster::Attributes::Arl::Id,
        BitFlags<AttributeQualityFlags>{ AttributeQualityFlags::kListAttribute },
        Access::Privilege::kView,
    }));
#endif

    return builder.AppendElements(GetGlobalAttributes());
}

ActionReturnStatus AccessControlAttribute::ReadAttribute(const ReadAttributeRequest & aRequest, AttributeValueEncoder & aEncoder)
{
    switch (aRequest.path.mAttributeId)
    {
    case AccessControlCluster::Attributes::Acl::Id:
        return ReadAcl(aEncoder);
    case AccessControlCluster::Attributes::Extension::Id:
        return ReadExtension(aEncoder);
    case AccessControlCluster::Attributes::SubjectsPerAccessControlEntry::Id: {
        size_t value = 0;
        ReturnErrorOnFailure(GetAccessControl().GetMaxSubjectsPerEntry(value));
        return aEncoder.Encode(static_cast<uint16_t>(value));
    }
    case AccessControlCluster::Attributes::TargetsPerAccessControlEntry::Id: {
        size_t value = 0;
        ReturnErrorOnFailure(GetAccessControl().GetMaxTargetsPerEntry(value));
        return aEncoder.Encode(static_cast<uint16_t>(value));
    }
    case AccessControlCluster::Attributes::AccessControlEntriesPerFabric::Id: {
        size_t value = 0;
        ReturnErrorOnFailure(GetAccessControl().GetMaxEntriesPerFabric(value));
        return aEncoder.Encode(static_cast<uint16_t>(value));
    }
#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
    case AccessControlCluster::Attributes::CommissioningARL::Id:
        return ReadCommissioningArl(aEncoder);
    case AccessControlCluster::Attributes::Arl::Id:
        return ReadArl(aEncoder);
#endif
    case AccessControlCluster::Attributes::ClusterRevision::Id:
        return aEncoder.Encode(kClusterRevision);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR AccessControlAttribute::ReadAcl(AttributeValueEncoder & aEncoder)
{
    AccessControl::EntryIterator iterator;
    AccessControl::Entry entry;
    AclStorage::EncodableEntry encodableEntry(entry);
    return aEncoder.EncodeList([&](const auto & encoder) -> CHIP_ERROR {
        for (auto & info : Server::GetInstance().GetFabricTable())
        {
            auto fabric = info.GetFabricIndex();
            ReturnErrorOnFailure(GetAccessControl().Entries(fabric, iterator));
            CHIP_ERROR err = CHIP_NO_ERROR;
            while ((err = iterator.Next(entry)) == CHIP_NO_ERROR)
            {
                ReturnErrorOnFailure(encoder.Encode(encodableEntry));
            }
            VerifyOrReturnError(err == CHIP_NO_ERROR || err == CHIP_ERROR_SENTINEL, err);
        }
        return CHIP_NO_ERROR;
    });
}

CHIP_ERROR AccessControlAttribute::ReadExtension(AttributeValueEncoder & aEncoder)
{
    auto & storage = Server::GetInstance().GetPersistentStorage();
    auto & fabrics = Server::GetInstance().GetFabricTable();

    return aEncoder.EncodeList([&](const auto & encoder) -> CHIP_ERROR {
        for (auto & fabric : fabrics)
        {
            uint8_t buffer[kExtensionDataMaxLength] = { 0 };
            uint16_t size                           = static_cast<uint16_t>(sizeof(buffer));
            CHIP_ERROR errStorage                   = storage.SyncGetKeyValue(
                DefaultStorageKeyAllocator::AccessControlExtensionEntry(fabric.GetFabricIndex()).KeyName(), buffer, size);
            VerifyOrReturnError(errStorage != CHIP_ERROR_BUFFER_TOO_SMALL, CHIP_ERROR_INCORRECT_STATE);
            if (errStorage == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND)
            {
                continue;
            }
            ReturnErrorOnFailure(errStorage);
            AccessControlCluster::Structs::AccessControlExtensionStruct::Type item = {
                .data        = ByteSpan(buffer, size),
                .fabricIndex = fabric.GetFabricIndex(),
            };
            ReturnErrorOnFailure(encoder.Encode(item));
        }
        return CHIP_NO_ERROR;
    });
}

ActionReturnStatus AccessControlAttribute::WriteAttribute(const DataModel::WriteAttributeRequest & aRequest,
                                                          AttributeValueDecoder & aDecoder)
{
    ActionReturnStatus status = Protocols::InteractionModel::Status::UnsupportedAttribute;

    switch (aRequest.path.mAttributeId)
    {
    case AccessControlCluster::Attributes::Acl::Id:
        status = ChipErrorToStatus(WriteAcl(aRequest.path, aDecoder));
        break;
    case AccessControlCluster::Attributes::Extension::Id:
        status = ChipErrorToStatus(WriteExtension(aRequest.path, aDecoder));
        break;
    }

    if (status.IsSuccess())
    {
        IncreaseDataVersion();
        // TODO: subscription notification that is sane?
        MatterReportingAttributeChangeCallback(aRequest.path);
    }

    return status;
}

CHIP_ERROR AccessControlAttribute::AcceptedCommands(const ConcreteClusterPath & path,
                                                    DataModel::ListBuilder<AcceptedCommandEntry> & builder)
{
#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
    static constexpr std::array<AcceptedCommandEntry, 1> kEntries{ { {
        AccessControlCluster::Commands::ReviewFabricRestrictions::Id,
        {},
        Access::Privilege::kAdminister,
    } } };

    return builder.ReferenceExisting(kEntries);
#else
    return CHIP_NO_ERROR;
#endif
}

CHIP_ERROR AccessControlAttribute::GeneratedCommands(const ConcreteClusterPath & path, DataModel::ListBuilder<CommandId> & builder)
{
#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
    static constexpr std::array<CommandId, 1> kEntries{ {
        AccessControlCluster::Commands::ReviewFabricRestrictionsResponse::Id,
    } };
    return builder.ReferenceExisting(kEntries);
#else
    return CHIP_NO_ERROR;
#endif
}

CHIP_ERROR AccessControlAttribute::WriteAcl(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder)
{
    FabricIndex accessingFabricIndex = aDecoder.AccessingFabricIndex();

    size_t oldCount;
    ReturnErrorOnFailure(GetAccessControl().GetEntryCount(accessingFabricIndex, oldCount));
    size_t maxCount;
    ReturnErrorOnFailure(GetAccessControl().GetMaxEntriesPerFabric(maxCount));

    if (!aPath.IsListItemOperation())
    {
        DataModel::DecodableList<AclStorage::DecodableEntry> list;
        ReturnErrorOnFailure(aDecoder.Decode(list));

        size_t newCount;
        ReturnErrorOnFailure(list.ComputeSize(&newCount));

        VerifyOrReturnError(newCount <= maxCount, CHIP_IM_GLOBAL_STATUS(ResourceExhausted));

        auto iterator = list.begin();
        size_t i      = 0;
        while (iterator.Next())
        {
            if (i < oldCount)
            {
                ReturnErrorOnFailure(GetAccessControl().UpdateEntry(&aDecoder.GetSubjectDescriptor(), accessingFabricIndex, i,
                                                                    iterator.GetValue().GetEntry()));
            }
            else
            {
                ReturnErrorOnFailure(GetAccessControl().CreateEntry(&aDecoder.GetSubjectDescriptor(), accessingFabricIndex, nullptr,
                                                                    iterator.GetValue().GetEntry()));
            }
            ++i;
        }
        ReturnErrorOnFailure(iterator.GetStatus());

        while (i < oldCount)
        {
            --oldCount;
            ReturnErrorOnFailure(GetAccessControl().DeleteEntry(&aDecoder.GetSubjectDescriptor(), accessingFabricIndex, oldCount));
        }
    }
    else if (aPath.mListOp == ConcreteDataAttributePath::ListOperation::AppendItem)
    {
        VerifyOrReturnError((oldCount + 1) <= maxCount, CHIP_IM_GLOBAL_STATUS(ResourceExhausted));

        AclStorage::DecodableEntry decodableEntry;
        ReturnErrorOnFailure(aDecoder.Decode(decodableEntry));

        ReturnErrorOnFailure(GetAccessControl().CreateEntry(&aDecoder.GetSubjectDescriptor(), accessingFabricIndex, nullptr,
                                                            decodableEntry.GetEntry()));
    }
    else
    {
        return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR AccessControlAttribute::WriteExtension(const ConcreteDataAttributePath & aPath, AttributeValueDecoder & aDecoder)
{
    auto & storage = Server::GetInstance().GetPersistentStorage();

    FabricIndex accessingFabricIndex = aDecoder.AccessingFabricIndex();

    uint8_t buffer[kExtensionDataMaxLength] = { 0 };
    uint16_t size                           = static_cast<uint16_t>(sizeof(buffer));
    CHIP_ERROR errStorage                   = storage.SyncGetKeyValue(
        DefaultStorageKeyAllocator::AccessControlExtensionEntry(accessingFabricIndex).KeyName(), buffer, size);
    VerifyOrReturnError(errStorage != CHIP_ERROR_BUFFER_TOO_SMALL, CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(errStorage == CHIP_NO_ERROR || errStorage == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND, errStorage);

    if (!aPath.IsListItemOperation())
    {
        DataModel::DecodableList<AccessControlCluster::Structs::AccessControlExtensionStruct::DecodableType> list;
        ReturnErrorOnFailure(aDecoder.Decode(list));

        size_t count = 0;
        ReturnErrorOnFailure(list.ComputeSize(&count));

        if (count == 0)
        {
            VerifyOrReturnError(errStorage != CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND, CHIP_NO_ERROR);
            ReturnErrorOnFailure(storage.SyncDeleteKeyValue(
                DefaultStorageKeyAllocator::AccessControlExtensionEntry(accessingFabricIndex).KeyName()));
            AccessControlCluster::Structs::AccessControlExtensionStruct::Type item = {
                .data        = ByteSpan(buffer, size),
                .fabricIndex = accessingFabricIndex,
            };
            ReturnErrorOnFailure(
                LogExtensionChangedEvent(item, aDecoder.GetSubjectDescriptor(), AccessControlCluster::ChangeTypeEnum::kRemoved));
        }
        else if (count == 1)
        {
            auto iterator = list.begin();
            if (!iterator.Next())
            {
                ReturnErrorOnFailure(iterator.GetStatus());
                // If counted an item, iterator doesn't return it, iterator has no error, that's bad.
                return CHIP_ERROR_INCORRECT_STATE;
            }
            auto & item = iterator.GetValue();
            // TODO(#13590): generated code doesn't automatically handle max length so do it manually
            VerifyOrReturnError(item.data.size() <= kExtensionDataMaxLength, CHIP_IM_GLOBAL_STATUS(ConstraintError));

            ReturnErrorOnFailure(CheckExtensionEntryDataFormat(item.data));

            ReturnErrorOnFailure(
                storage.SyncSetKeyValue(DefaultStorageKeyAllocator::AccessControlExtensionEntry(accessingFabricIndex).KeyName(),
                                        item.data.data(), static_cast<uint16_t>(item.data.size())));
            ReturnErrorOnFailure(LogExtensionChangedEvent(item, aDecoder.GetSubjectDescriptor(),
                                                          errStorage == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND
                                                              ? AccessControlCluster::ChangeTypeEnum::kAdded
                                                              : AccessControlCluster::ChangeTypeEnum::kChanged));
        }
        else
        {
            return CHIP_IM_GLOBAL_STATUS(ConstraintError);
        }
    }
    else if (aPath.mListOp == ConcreteDataAttributePath::ListOperation::AppendItem)
    {
        VerifyOrReturnError(errStorage == CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND, CHIP_IM_GLOBAL_STATUS(ConstraintError));
        AccessControlCluster::Structs::AccessControlExtensionStruct::DecodableType item;
        ReturnErrorOnFailure(aDecoder.Decode(item));
        // TODO(#13590): generated code doesn't automatically handle max length so do it manually
        VerifyOrReturnError(item.data.size() <= kExtensionDataMaxLength, CHIP_IM_GLOBAL_STATUS(ConstraintError));

        ReturnErrorOnFailure(CheckExtensionEntryDataFormat(item.data));

        ReturnErrorOnFailure(
            storage.SyncSetKeyValue(DefaultStorageKeyAllocator::AccessControlExtensionEntry(accessingFabricIndex).KeyName(),
                                    item.data.data(), static_cast<uint16_t>(item.data.size())));
        ReturnErrorOnFailure(
            LogExtensionChangedEvent(item, aDecoder.GetSubjectDescriptor(), AccessControlCluster::ChangeTypeEnum::kAdded));
    }
    else
    {
        return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
    }

    return CHIP_NO_ERROR;
}

void AccessControlAttribute::OnEntryChanged(const SubjectDescriptor * subjectDescriptor, FabricIndex fabric, size_t index,
                                            const AccessControl::Entry * entry, AccessControl::EntryListener::ChangeType changeType)
{
    // NOTE: If the entry was changed internally by the system (e.g. creating
    // entries at startup from persistent storage, or deleting entries when a
    // fabric is removed), then there won't be a subject descriptor, and also
    // it won't be appropriate to create an event.
    if (subjectDescriptor == nullptr)
    {
        return;
    }

    CHIP_ERROR err;
    AclEvent event{ .changeType = ChangeTypeEnum::kChanged, .fabricIndex = subjectDescriptor->fabricIndex };

    if (changeType == AccessControl::EntryListener::ChangeType::kAdded)
    {
        event.changeType = ChangeTypeEnum::kAdded;
    }
    else if (changeType == AccessControl::EntryListener::ChangeType::kRemoved)
    {
        event.changeType = ChangeTypeEnum::kRemoved;
    }

    if (subjectDescriptor->authMode == Access::AuthMode::kCase)
    {
        event.adminNodeID.SetNonNull(subjectDescriptor->subject);
    }
    else if (subjectDescriptor->authMode == Access::AuthMode::kPase)
    {
        event.adminPasscodeID.SetNonNull(PAKEKeyIdFromNodeId(subjectDescriptor->subject));
    }

    EventNumber eventNumber;

    if (entry != nullptr)
    {
        // NOTE: don't destroy encodable entry before staging entry is used!
        AclStorage::EncodableEntry encodableEntry(*entry);
        SuccessOrExit(err = encodableEntry.Stage());
        event.latestValue.SetNonNull(encodableEntry.GetStagingEntry());
        SuccessOrExit(err = LogEvent(event, 0, eventNumber));
    }
    else
    {
        SuccessOrExit(err = LogEvent(event, 0, eventNumber));
    }

    return;

exit:
    ChipLogError(DataManagement, "AccessControlCluster: event failed %" CHIP_ERROR_FORMAT, err.Format());
}

#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
CHIP_ERROR AccessControlAttribute::ReadCommissioningArl(AttributeValueEncoder & aEncoder)
{
    auto accessRestrictionProvider = GetAccessControl().GetAccessRestrictionProvider();

    return aEncoder.EncodeList([&](const auto & encoder) -> CHIP_ERROR {
        if (accessRestrictionProvider != nullptr)
        {
            auto entries = accessRestrictionProvider->GetCommissioningEntries();

            for (auto & entry : entries)
            {
                ArlEncoder::CommissioningEncodableEntry encodableEntry(entry);
                ReturnErrorOnFailure(encoder.Encode(encodableEntry));
            }
        }
        return CHIP_NO_ERROR;
    });
}

CHIP_ERROR AccessControlAttribute::ReadArl(AttributeValueEncoder & aEncoder)
{
    auto accessRestrictionProvider = GetAccessControl().GetAccessRestrictionProvider();

    return aEncoder.EncodeList([&](const auto & encoder) -> CHIP_ERROR {
        if (accessRestrictionProvider != nullptr)
        {
            for (const auto & info : Server::GetInstance().GetFabricTable())
            {
                auto fabric = info.GetFabricIndex();
                // get entries for fabric
                std::vector<AccessRestrictionProvider::Entry> entries;
                ReturnErrorOnFailure(accessRestrictionProvider->GetEntries(fabric, entries));
                for (const auto & entry : entries)
                {
                    ArlEncoder::EncodableEntry encodableEntry(entry);
                    ReturnErrorOnFailure(encoder.Encode(encodableEntry));
                }
            }
        }
        return CHIP_NO_ERROR;
    });
}
void AccessControlAttribute::MarkCommissioningRestrictionListChanged()
{
    MatterReportingAttributeChangeCallback(kRootEndpointId, AccessControlCluster::Id,
                                           AccessControlCluster::Attributes::CommissioningARL::Id);
}

void AccessControlAttribute::MarkRestrictionListChanged(FabricIndex fabricIndex)
{
    MatterReportingAttributeChangeCallback(kRootEndpointId, AccessControlCluster::Id, AccessControlCluster::Attributes::Arl::Id);
}

void AccessControlAttribute::OnFabricRestrictionReviewUpdate(FabricIndex fabricIndex, uint64_t token,
                                                             Optional<CharSpan> instruction, Optional<CharSpan> arlRequestFlowUrl)
{
    CHIP_ERROR err;
    ArlReviewEvent event{ .token = token, .fabricIndex = fabricIndex };

    event.instruction       = instruction;
    event.ARLRequestFlowUrl = arlRequestFlowUrl;

    EventNumber eventNumber;
    SuccessOrExit(err = LogEvent(event, kRootEndpointId, eventNumber));

    return;

exit:
    ChipLogError(DataManagement, "AccessControlCluster: review event failed: %" CHIP_ERROR_FORMAT, err.Format());
}
#endif

} // namespace

void MatterAccessControlPluginServerInitCallback()
{
    ChipLogProgress(DataManagement, "AccessControlCluster: initializing");

    CHIP_ERROR err = ServerClusterInterfaceRegistry::Instance().Register(0 /* endpointId */, &sAttribute);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "Failed to register ACL: %" CHIP_ERROR_FORMAT, err.Format());
    }

    GetAccessControl().AddEntryListener(sAttribute);

#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
    auto accessRestrictionProvider = GetAccessControl().GetAccessRestrictionProvider();
    if (accessRestrictionProvider != nullptr)
    {
        accessRestrictionProvider->AddListener(sAttribute);
    }
#endif
}

#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
bool emberAfAccessControlClusterReviewFabricRestrictionsCallback(
    CommandHandler * commandObj, const ConcreteCommandPath & commandPath,
    const Clusters::AccessControl::Commands::ReviewFabricRestrictions::DecodableType & commandData)
{
    if (commandPath.mEndpointId != kRootEndpointId)
    {
        ChipLogError(DataManagement, "AccessControlCluster: invalid endpoint in ReviewFabricRestrictions request");
        commandObj->AddStatus(commandPath, Protocols::InteractionModel::Status::InvalidCommand);
        return true;
    }

    uint64_t token;
    std::vector<AccessRestrictionProvider::Entry> entries;
    auto entryIter = commandData.arl.begin();
    while (entryIter.Next())
    {
        AccessRestrictionProvider::Entry entry;
        entry.fabricIndex    = commandObj->GetAccessingFabricIndex();
        entry.endpointNumber = entryIter.GetValue().endpoint;
        entry.clusterId      = entryIter.GetValue().cluster;

        auto restrictionIter = entryIter.GetValue().restrictions.begin();
        while (restrictionIter.Next())
        {
            AccessRestrictionProvider::Restriction restriction;
            if (ArlEncoder::Convert(restrictionIter.GetValue().type, restriction.restrictionType) != CHIP_NO_ERROR)
            {
                ChipLogError(DataManagement, "AccessControlCluster: invalid restriction type conversion");
                commandObj->AddStatus(commandPath, Protocols::InteractionModel::Status::InvalidCommand);
                return true;
            }

            if (!restrictionIter.GetValue().id.IsNull())
            {
                restriction.id.SetValue(restrictionIter.GetValue().id.Value());
            }
            entry.restrictions.push_back(restriction);
        }

        if (restrictionIter.GetStatus() != CHIP_NO_ERROR)
        {
            ChipLogError(DataManagement, "AccessControlCluster: invalid ARL data");
            commandObj->AddStatus(commandPath, Protocols::InteractionModel::Status::InvalidCommand);
            return true;
        }

        entries.push_back(entry);
    }

    if (entryIter.GetStatus() != CHIP_NO_ERROR)
    {
        ChipLogError(DataManagement, "AccessControlCluster: invalid ARL data");
        commandObj->AddStatus(commandPath, Protocols::InteractionModel::Status::InvalidCommand);
        return true;
    }

    CHIP_ERROR err = GetAccessControl().GetAccessRestrictionProvider()->RequestFabricRestrictionReview(
        commandObj->GetAccessingFabricIndex(), entries, token);

    if (err == CHIP_NO_ERROR)
    {
        Clusters::AccessControl::Commands::ReviewFabricRestrictionsResponse::Type response;
        response.token = token;
        commandObj->AddResponse(commandPath, response);
    }
    else
    {
        ChipLogError(DataManagement, "AccessControlCluster: restriction review failed: %" CHIP_ERROR_FORMAT, err.Format());
        commandObj->AddStatus(commandPath, Protocols::InteractionModel::ClusterStatusCode(err));
    }

    return true;
}
#endif

std::optional<ActionReturnStatus> AccessControlAttribute::InvokeCommand(const InvokeRequest & request,
                                                                        chip::TLV::TLVReader & input_arguments,
                                                                        CommandHandler * handler)
{
#if CHIP_CONFIG_USE_ACCESS_RESTRICTIONS
    if (request.path.mCommandId == AccessControlCluster::Commands::ReviewFabricRestrictions::Id)
    {
        Clusters::AccessControl::Commands::ReviewFabricRestrictions::DecodableType input;
        ReturnErrorOnFailure(input.Decode(input_arguments));
        if (!emberAfAccessControlClusterReviewFabricRestrictionsCallback(handler, request.path, input))
        {
            return Protocols::InteractionModel::Status::Failure;
        }
        return std::nullopt;
    }
#endif

    return Protocols::InteractionModel::Status::UnsupportedCommand;
}
