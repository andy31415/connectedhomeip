/**
 *    Copyright (c) 2020-2026 Project CHIP Authors
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
#include <app/clusters/groups-server/GroupsCluster.h>

#include <app/clusters/scenes-server/Constants.h>
#include <app/server-cluster/AttributeListBuilder.h>
#include <clusters/GroupKeyManagement/Ids.h>
#include <clusters/Groups/Attributes.h>
#include <clusters/Groups/Commands.h>
#include <clusters/Groups/Metadata.h>
#include <clusters/Groups/Structs.h>
#include <tracing/macros.h>

using namespace chip::app::Clusters::Groups;
using chip::Credentials::GroupDataProvider;
using chip::Protocols::InteractionModel::Status;

namespace chip::app::Clusters {
namespace {

/**
 * @brief Checks if there are key set associated with the given GroupId
 */
bool KeyExists(GroupDataProvider & provider, FabricIndex fabricIndex, GroupId groupId)
{
    GroupDataProvider::GroupKey entry;

    auto it    = provider.IterateGroupKeys(fabricIndex);
    bool found = false;
    while (!found && it->Next(entry))
    {
        if (entry.group_id == groupId)
        {
            GroupDataProvider::KeySet keys;
            found = (CHIP_NO_ERROR == provider.GetKeySet(fabricIndex, entry.keyset_id, keys));
        }
    }
    it->Release();

    return found;
}

struct GroupMembershipResponse
{
    // A null capacity means that it is unknown if any further groups MAY be added.
    const chip::app::DataModel::Nullable<uint8_t> kCapacityUnknown;

    // Use GetCommandId instead of commandId directly to avoid naming conflict with CommandIdentification in ExecutionOfACommand
    static constexpr CommandId GetCommandId() { return Commands::GetGroupMembershipResponse::Id; }
    static constexpr ClusterId GetClusterId() { return Groups::Id; }

    GroupMembershipResponse(const Commands::GetGroupMembership::DecodableType & data, chip::EndpointId endpoint,
                            GroupDataProvider::EndpointIterator * iter) :
        mCommandData(data),
        mEndpoint(endpoint), mIterator(iter)
    {}

    const Commands::GetGroupMembership::DecodableType & mCommandData;
    chip::EndpointId mEndpoint                      = kInvalidEndpointId;
    GroupDataProvider::EndpointIterator * mIterator = nullptr;

    CHIP_ERROR Encode(TLV::TLVWriter & writer, TLV::Tag tag) const
    {
        TLV::TLVType outer;

        ReturnErrorOnFailure(writer.StartContainer(tag, TLV::kTLVType_Structure, outer));

        ReturnErrorOnFailure(app::DataModel::Encode(
            writer, TLV::ContextTag(Commands::GetGroupMembershipResponse::Fields::kCapacity), kCapacityUnknown));
        {
            TLV::TLVType type;
            ReturnErrorOnFailure(writer.StartContainer(TLV::ContextTag(Commands::GetGroupMembershipResponse::Fields::kGroupList),
                                                       TLV::kTLVType_Array, type));
            {
                GroupDataProvider::GroupEndpoint mapping;
                size_t requestedCount = 0;
                ReturnErrorOnFailure(mCommandData.groupList.ComputeSize(&requestedCount));

                if (0 == requestedCount)
                {
                    // 1.3.6.3.1. If the GroupList field is empty, the entity SHALL respond with all group identifiers of which the
                    // entity is a member.
                    while (mIterator && mIterator->Next(mapping))
                    {
                        if (mapping.endpoint_id == mEndpoint)
                        {
                            ReturnErrorOnFailure(app::DataModel::Encode(writer, TLV::AnonymousTag(), mapping.group_id));
                            ChipLogDetail(Zcl, " 0x%02x", mapping.group_id);
                        }
                    }
                }
                else
                {
                    while (mIterator && mIterator->Next(mapping))
                    {
                        auto iter = mCommandData.groupList.begin();
                        while (iter.Next())
                        {
                            if (mapping.endpoint_id == mEndpoint && mapping.group_id == iter.GetValue())
                            {
                                ReturnErrorOnFailure(app::DataModel::Encode(writer, TLV::AnonymousTag(), mapping.group_id));
                                ChipLogDetail(Zcl, " 0x%02x", mapping.group_id);
                                break;
                            }
                        }
                        ReturnErrorOnFailure(iter.GetStatus());
                    }
                }
                ChipLogDetail(Zcl, "]");
            }
            ReturnErrorOnFailure(writer.EndContainer(type));
        }
        ReturnErrorOnFailure(writer.EndContainer(outer));
        return CHIP_NO_ERROR;
    }
};

} // namespace

CHIP_ERROR GroupsCluster::Attributes(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder)
{
    AttributeListBuilder listBuilder(builder);
    return listBuilder.Append(Attributes::kMandatoryMetadata, {});
}

CHIP_ERROR GroupsCluster::AcceptedCommands(const ConcreteClusterPath & path,
                                           ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> & builder)
{
    using namespace Commands;

    static constexpr DataModel::AcceptedCommandEntry kAcceptedCommands[] = {
        AddGroup::kMetadataEntry,              //
        ViewGroup::kMetadataEntry,             //
        GetGroupMembership::kMetadataEntry,    //
        RemoveGroup::kMetadataEntry,           //
        RemoveAllGroups::kMetadataEntry,       //
        AddGroupIfIdentifying::kMetadataEntry, //
    };

    return builder.ReferenceExisting(kAcceptedCommands);
}

CHIP_ERROR GroupsCluster::GeneratedCommands(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<CommandId> & builder)
{
    using namespace Commands;

    static constexpr CommandId kGeneratedCommands[] = {
        AddGroupResponse::Id,           //
        ViewGroupResponse::Id,          //
        GetGroupMembershipResponse::Id, //
        RemoveGroupResponse::Id,        //
    };

    return builder.ReferenceExisting(kGeneratedCommands);
}

DataModel::ActionReturnStatus GroupsCluster::ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                           AttributeValueEncoder & encoder)
{
    using namespace Attributes;

    switch (request.path.mAttributeId)
    {
    case ClusterRevision::Id:
        return encoder.Encode(kRevision);
    case FeatureMap::Id:
        // Group names is hardcoded (feature is M conformance in the spec)
        return encoder.Encode(Feature::kGroupNames);
    case NameSupport::Id:
        // According to the spec, kGroupNames must be set (M conformance in the spec)
        return encoder.Encode(NameSupportBitmap::kGroupNames);
    default:
        return Status::UnsupportedAttribute;
    }
}

std::optional<DataModel::ActionReturnStatus> GroupsCluster::InvokeCommand(const DataModel::InvokeRequest & request,
                                                                          chip::TLV::TLVReader & input_arguments,
                                                                          CommandHandler * handler)
{
    using namespace Commands;

    switch (request.path.mCommandId)
    {
    case AddGroup::Id: {
        AddGroup::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));

        Groups::Commands::AddGroupResponse::Type response;
        response.groupID = request_data.groupID;
        response.status  = to_underlying(HandleAddGroup(request_data, request.GetAccessingFabricIndex()));
        handler->AddResponse(request.path, response);

        return std::nullopt;
    }
    case ViewGroup::Id: {
        ViewGroup::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));

        return HandleViewGroup(request_data, handler);
    }
    case GetGroupMembership::Id: {
        GetGroupMembership::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));

        return HandleGetGroupMembership(request_data, handler);
    }
    case RemoveGroup::Id: {
        RemoveGroup::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));

        Groups::Commands::RemoveGroupResponse::Type response;
        response.groupID = request_data.groupID;
        response.status  = to_underlying(HandleRemoveGroup(request_data, request.GetAccessingFabricIndex()));
        handler->AddResponse(request.path, response);
        return std::nullopt;
    }
    case RemoveAllGroups::Id: {
        return HandleRemoveAllGroups(request.GetAccessingFabricIndex());
    }
    case AddGroupIfIdentifying::Id: {
        AddGroupIfIdentifying::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));
        // FIXME: implement
        return Status::UnsupportedCommand;
    }
    default:
        return Status::UnsupportedCommand;
    }
}

void GroupsCluster::NotifyGroupTableChanged()
{
    // TODO: This seems a bit coupled: we are notifying in this cluster that ANOTHER cluster
    //       has changed. We should support only one cluster or another really...
    VerifyOrReturn(mContext != nullptr);
    mContext->interactionContext.dataModelChangeListener.MarkDirty(
        { mPath.mEndpointId, GroupKeyManagement::Id, GroupKeyManagement::Attributes::GroupTable::Id });
}

Protocols::InteractionModel::Status GroupsCluster::HandleAddGroup(const Groups::Commands::AddGroup::DecodableType & input,
                                                                  FabricIndex fabricIndex)
{
    VerifyOrReturnError(IsValidGroupId(input.groupID), Status::ConstraintError);
    VerifyOrReturnError(input.groupName.size() <= GroupDataProvider::GroupInfo::kGroupNameMax, Status::ConstraintError);

    VerifyOrReturnError(KeyExists(mGroupDataProvider, fabricIndex, input.groupID), Status::UnsupportedAccess);

    // Add a new entry to the GroupTable
    if (CHIP_ERROR err = mGroupDataProvider.SetGroupInfo(fabricIndex, GroupDataProvider::GroupInfo(input.groupID, input.groupName));
        err != CHIP_NO_ERROR)
    {
        ChipLogDetail(Zcl, "ERR: Failed to add mapping (end:%d, group:0x%x), err:%" CHIP_ERROR_FORMAT, mPath.mEndpointId,
                      input.groupID, err.Format());
        return Status::ResourceExhausted;
    }

    if (CHIP_ERROR err = mGroupDataProvider.AddEndpoint(fabricIndex, input.groupID, mPath.mEndpointId); err != CHIP_NO_ERROR)
    {
        ChipLogDetail(Zcl, "ERR: Failed to add mapping (end:%d, group:0x%x), err:%" CHIP_ERROR_FORMAT, mPath.mEndpointId,
                      input.groupID, err.Format());
        return Status::ResourceExhausted;
    }
    NotifyGroupTableChanged();
    return Status::Success;
}

Protocols::InteractionModel::Status GroupsCluster::HandleRemoveGroup(const Groups::Commands::RemoveGroup::DecodableType & input,
                                                                     FabricIndex fabricIndex)
{
    VerifyOrReturnError(IsValidGroupId(input.groupID), Status::ConstraintError);
    VerifyOrReturnError(mGroupDataProvider.HasEndpoint(fabricIndex, input.groupID, mPath.mEndpointId), Status::NotFound);

    if (CHIP_ERROR err = mGroupDataProvider.RemoveEndpoint(fabricIndex, input.groupID, mPath.mEndpointId); err != CHIP_NO_ERROR)
    {
        ChipLogDetail(Zcl, "ERR: Failed to remove mapping (end:%d, group:0x%x), err:%" CHIP_ERROR_FORMAT, mPath.mEndpointId,
                      input.groupID, err.Format());
        return Status::NotFound;
    }

    NotifyGroupTableChanged();
    return Status::Success;
}

std::optional<DataModel::ActionReturnStatus>
GroupsCluster::HandleViewGroup(const Groups::Commands::ViewGroup::DecodableType & input, CommandHandler * handler)
{
    MATTER_TRACE_SCOPE("ViewGroup", "Groups");
    auto fabricIndex = handler->GetAccessingFabricIndex();
    auto groupId     = input.groupID;
    GroupDataProvider::GroupInfo info;
    Groups::Commands::ViewGroupResponse::Type response;
    Status status = Status::NotFound;

    VerifyOrExit(IsValidGroupId(groupId), status = Status::ConstraintError);
    VerifyOrExit(mGroupDataProvider.HasEndpoint(fabricIndex, groupId, mPath.mEndpointId), status = Status::NotFound);
    VerifyOrExit(CHIP_NO_ERROR == mGroupDataProvider.GetGroupInfo(fabricIndex, groupId, info), status = Status::NotFound);

    response.groupName = CharSpan(info.name, strnlen(info.name, GroupDataProvider::GroupInfo::kGroupNameMax));
    status             = Status::Success;
exit:
    response.groupID = groupId;
    response.status  = to_underlying(status);
    handler->AddResponse({ mPath.mEndpointId, mPath.mClusterId, Commands::RemoveGroup::Id }, response);
    return std::nullopt;
}

std::optional<DataModel::ActionReturnStatus>
GroupsCluster::HandleGetGroupMembership(const Groups::Commands::GetGroupMembership::DecodableType & input, CommandHandler * handler)
{
    MATTER_TRACE_SCOPE("GetGroupMembership", "Groups");
    GroupDataProvider::EndpointIterator * iter = mGroupDataProvider.IterateEndpoints(handler->GetAccessingFabricIndex());
    VerifyOrReturnError(nullptr != iter, Status::Failure);

    handler->AddResponse({ mPath.mEndpointId, mPath.mClusterId, Commands::GetGroupMembership::Id },
                         GroupMembershipResponse(input, mPath.mEndpointId, iter));
    iter->Release();
    return std::nullopt;
}

std::optional<DataModel::ActionReturnStatus> GroupsCluster::HandleRemoveAllGroups(FabricIndex fabricIndex)
{
    MATTER_TRACE_SCOPE("RemoveAllGroups", "Groups");

    if (mScenesIngeration != nullptr)
    {
        GroupDataProvider::EndpointIterator * iter = mGroupDataProvider.IterateEndpoints(fabricIndex);
        GroupDataProvider::GroupEndpoint mapping;

        VerifyOrReturnError(nullptr != iter, Status::Failure);
        while (iter->Next(mapping))
        {
            if (mPath.mEndpointId == mapping.endpoint_id)
            {
                LogErrorOnFailure(mScenesIngeration->GroupWillBeRemoved(fabricIndex, mapping.group_id));
            }
        }
        iter->Release();
        LogErrorOnFailure(mScenesIngeration->GroupWillBeRemoved(fabricIndex, scenes::kGlobalSceneGroupId));
    }

    LogErrorOnFailure(mGroupDataProvider.RemoveEndpoint(fabricIndex, mPath.mEndpointId));
    NotifyGroupTableChanged();
    return Status::Success;
}

// TODO: AddGroupIfIdentifying

} // namespace chip::app::Clusters

#if 0

#ifdef MATTER_DM_PLUGIN_SCENES_MANAGEMENT
#include <app/clusters/scenes-server/scenes-server.h> // nogncheck
#endif                                                // MATTER_DM_PLUGIN_SCENES_MANAGEMENT

using namespace chip;
using namespace chip::app::Clusters;
using namespace app::Clusters;
using namespace app::Clusters::Groups;
using namespace chip::Credentials;
using Protocols::InteractionModel::Status;

// Is the device identifying?
static bool emberAfIsDeviceIdentifying(EndpointId endpoint)
{
#ifdef ZCL_USING_IDENTIFY_CLUSTER_SERVER
    auto cluster = FindIdentifyClusterOnEndpoint(endpoint);
    return cluster != nullptr && cluster->GetIdentifyTime() > 0;
#else
    return false;
#endif
}

bool emberAfGroupsClusterAddGroupIfIdentifyingCallback(app::CommandHandler * commandObj,
                                                       const app::ConcreteCommandPath & commandPath,
                                                       const Commands::AddGroupIfIdentifying::DecodableType & commandData)
{
    MATTER_TRACE_SCOPE("AddGroupIfIdentifying", "Groups");
    auto fabricIndex = commandObj->GetAccessingFabricIndex();
    auto groupId     = commandData.groupID;
    auto groupName   = commandData.groupName;
    auto endpointId  = commandPath.mEndpointId;

    Status status;
    if (!emberAfIsDeviceIdentifying(endpointId))
    {
        // If not identifying, ignore add group -> success; not a failure.
        status = Status::Success;
    }
    else
    {
        status = GroupAdd(fabricIndex, endpointId, groupId, groupName);
    }

    commandObj->AddStatus(commandPath, status);
    return true;
}

#endif
