/*
 *
 *    Copyright (c) 2021-2023 Project CHIP Authors
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

#include "ScenesManagementCluster.h"

#include <clusters/ScenesManagement/Metadata.h>
#include <tracing/macros.h>

#include <optional>

using SceneTableEntry   = chip::scenes::DefaultSceneTableImpl::SceneTableEntry;
using SceneStorageId    = chip::scenes::DefaultSceneTableImpl::SceneStorageId;
using SceneData         = chip::scenes::DefaultSceneTableImpl::SceneData;
using ExtensionFieldSet = chip::scenes::ExtensionFieldSet;
using GroupDataProvider = chip::Credentials::GroupDataProvider;
using SceneTable        = chip::scenes::SceneTable<chip::scenes::ExtensionFieldSetsImpl>;
using AuthMode          = chip::Access::AuthMode;
using chip::Protocols::InteractionModel::Status;

using namespace chip::app::Clusters::ScenesManagement::Attributes;
using namespace chip::app::Clusters::ScenesManagement::Commands;
using namespace chip::app::Clusters::ScenesManagement::Structs;

namespace chip::app::Clusters {

namespace {

Protocols::InteractionModel::Status ResponseStatus(CHIP_ERROR err)
{
    // TODO : Properly fix mapping between error types (issue https://github.com/project-chip/connectedhomeip/issues/26885)
    if (CHIP_ERROR_NOT_FOUND == err)
    {
        return Protocols::InteractionModel::Status::NotFound;
    }
    if (CHIP_ERROR_NO_MEMORY == err)
    {
        return Protocols::InteractionModel::Status::ResourceExhausted;
    }
    if (CHIP_IM_GLOBAL_STATUS(UnsupportedAttribute) == err)
    {
        // TODO: Confirm if we need to add UnsupportedAttribute status as a return for Scene Commands
        return Protocols::InteractionModel::Status::InvalidCommand;
    }
    return StatusIB(err).mStatus;
}

/// Returns true if err is CHIP_NO_ERROR.
/// Otherwise it fills resp.status with the underlying error code.
template <typename ResponseType>
bool ValidateSuccess(CHIP_ERROR err, ResponseType & resp)
{
    VerifyOrReturnValue(CHIP_NO_ERROR != err, true);
    resp.status = to_underlying(ResponseStatus(err));
    return false;
}

} // namespace

CHIP_ERROR ScenesManagementCluster::UpdateFabricSceneInfo(FabricIndex fabric, Optional<GroupId> group, Optional<SceneId> scene,
                                                          Optional<bool> sceneValid)
{
    VerifyOrReturnError(kUndefinedFabricIndex != fabric, CHIP_ERROR_INVALID_ARGUMENT);

    SceneTable * sceneTable           = scenes::GetSceneTableImpl(mPath.mEndpointId);
    SceneInfoStruct::Type * sceneInfo = GetSceneInfoStruct(fabric);
    if (nullptr != sceneInfo)
    {
        if (group.HasValue())
        {
            sceneInfo->currentGroup = group.Value();
        }

        if (scene.HasValue())
        {
            sceneInfo->currentScene = scene.Value();
        }

        if (sceneValid.HasValue())
        {
            sceneInfo->sceneValid = sceneValid.Value();
        }

        ReturnErrorOnFailure(sceneTable->GetFabricSceneCount(fabric, sceneInfo->sceneCount));
        ReturnErrorOnFailure(sceneTable->GetRemainingCapacity(fabric, sceneInfo->remainingCapacity));
    }
    else
    {
        // If we couldn't find a SceneInfoStruct for the fabric, create one
        SceneInfoStruct::Type newSceneInfo;
        newSceneInfo.fabricIndex = fabric;

        newSceneInfo.currentGroup = group.ValueOr(0);
        newSceneInfo.currentScene = scene.ValueOr(0);
        newSceneInfo.sceneValid   = sceneValid.ValueOr(false);

        ReturnErrorOnFailure(sceneTable->GetFabricSceneCount(fabric, newSceneInfo.sceneCount));
        ReturnErrorOnFailure(sceneTable->GetRemainingCapacity(fabric, newSceneInfo.remainingCapacity));
        ReturnErrorOnFailure(SetSceneInfoStruct(fabric, newSceneInfo));
    }

    NotifyAttributeChanged(ScenesManagement::Attributes::FabricSceneInfo::Id);

    return CHIP_NO_ERROR;
}

SceneInfoStruct::Type * ScenesManagementCluster::FabricSceneInfo::GetSceneInfoStruct(FabricIndex fabric)
{
    uint8_t sceneInfoStructIndex = 0;
    VerifyOrReturnValue(CHIP_NO_ERROR == FindSceneInfoStructIndex(fabric, sceneInfoStructIndex), nullptr);
    return &mSceneInfoStructs[sceneInfoStructIndex];
}

/// @brief Sets the SceneInfoStruct for a specific fabric for a specific endpoint
/// @param endpoint target endpoint
/// @param fabric target fabric
/// @param [in] sceneInfoStruct SceneInfoStruct to set
/// @return CHIP_NO_ERROR, CHIP_ERROR_NOT_FOUND if the endpoint is not found, CHIP_ERROR_NO_MEMORY if the number of fabrics is
/// exceeded, CHIP_ERROR_INVALID_ARGUMENT if invalid fabric or endpoint
CHIP_ERROR ScenesManagementCluster::FabricSceneInfo::SetSceneInfoStruct(FabricIndex fabric,
                                                                        const SceneInfoStruct::Type & sceneInfoStruct)
{
    VerifyOrReturnError(kUndefinedFabricIndex != fabric, CHIP_ERROR_INVALID_ARGUMENT);

    uint8_t sceneInfoStructIndex = 0;
    if (CHIP_ERROR_NOT_FOUND == FindSceneInfoStructIndex(fabric, sceneInfoStructIndex))
    {
        VerifyOrReturnError(mSceneInfoStructsCount < MATTER_ARRAY_SIZE(mSceneInfoStructs), CHIP_ERROR_NO_MEMORY);
        sceneInfoStructIndex = mSceneInfoStructsCount;
        mSceneInfoStructsCount++;
    }
    mSceneInfoStructs[sceneInfoStructIndex] = sceneInfoStruct;
    return CHIP_NO_ERROR;
}

void ScenesManagementCluster::FabricSceneInfo::ClearSceneInfoStruct(FabricIndex fabric)
{
    uint8_t sceneInfoStructIndex = 0;
    ReturnOnFailure(FindSceneInfoStructIndex(fabric, sceneInfoStructIndex));

    uint8_t nextIndex = static_cast<uint8_t>(sceneInfoStructIndex + 1);
    uint8_t moveNum   = static_cast<uint8_t>(MATTER_ARRAY_SIZE(mSceneInfoStructs) - nextIndex);
    // Compress the endpoint's SceneInfoStruct array
    if (moveNum)
    {
        for (size_t i = 0; i < moveNum; ++i)
        {
            mSceneInfoStructs[sceneInfoStructIndex + i] = mSceneInfoStructs[nextIndex + i];
        }
    }

    // Decrement the SceneInfoStruct count
    mSceneInfoStructsCount--;

    // Clear the last populated SceneInfoStruct
    mSceneInfoStructs[mSceneInfoStructsCount].fabricIndex       = kUndefinedFabricIndex;
    mSceneInfoStructs[mSceneInfoStructsCount].sceneCount        = 0;
    mSceneInfoStructs[mSceneInfoStructsCount].currentScene      = 0;
    mSceneInfoStructs[mSceneInfoStructsCount].currentGroup      = 0;
    mSceneInfoStructs[mSceneInfoStructsCount].remainingCapacity = 0;
}

CHIP_ERROR ScenesManagementCluster::FabricSceneInfo::FindSceneInfoStructIndex(FabricIndex fabric, uint8_t & index)
{
    VerifyOrReturnError(kUndefinedFabricIndex != fabric, CHIP_ERROR_INVALID_ARGUMENT);

    index = 0;

    for (auto & info : mSceneInfoStructs)
    {
        if (info.fabricIndex == fabric)
        {
            return CHIP_NO_ERROR;
        }
        index++;
    }

    return CHIP_ERROR_NOT_FOUND;
}

CHIP_ERROR ScenesManagementCluster::Attributes(const ConcreteClusterPath & path,
                                               ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder)
{
    return builder.ReferenceExisting(ScenesManagement::Attributes::kMandatoryMetadata);
}

CHIP_ERROR ScenesManagementCluster::AcceptedCommands(const ConcreteClusterPath & path,
                                                     ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> & builder)
{
    if (mSupportCopyScenes)
    {
        ReturnErrorOnFailure(builder.EnsureAppendCapacity(1));
        ReturnErrorOnFailure(builder.Append(CopyScene::kMetadataEntry));
    }

    static constexpr DataModel::AcceptedCommandEntry kCommands[] = {
        AddScene::kMetadataEntry,           //
        ViewScene::kMetadataEntry,          //
        RemoveScene::kMetadataEntry,        //
        RemoveAllScenes::kMetadataEntry,    //
        StoreScene::kMetadataEntry,         //
        RecallScene::kMetadataEntry,        //
        GetSceneMembership::kMetadataEntry, //
    };

    return builder.ReferenceExisting(kCommands);
}

CHIP_ERROR ScenesManagementCluster::GeneratedCommands(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<CommandId> & builder)
{
    if (mSupportCopyScenes)
    {
        ReturnErrorOnFailure(builder.EnsureAppendCapacity(1));
        ReturnErrorOnFailure(builder.Append(CopySceneResponse::Id));
    }

    static constexpr CommandId kCommands[] = {
        AddSceneResponse::Id,           //
        ViewSceneResponse::Id,          //
        RemoveSceneResponse::Id,        //
        RemoveAllScenesResponse::Id,    //
        StoreSceneResponse::Id,         //
        GetSceneMembershipResponse::Id, //
    };

    return builder.ReferenceExisting(kCommands);
}

#if 0
class ScenesClusterFabricDelegate : public chip::FabricTable::Delegate
{
    void OnFabricRemoved(const FabricTable & fabricTable, FabricIndex fabricIndex) override
    {
        SceneTable * sceneTable = scenes::GetSceneTableImpl();
        VerifyOrReturn(nullptr != sceneTable);
        // The implementation of SceneTable::RemoveFabric() must not call back into the FabricTable
        TEMPORARY_RETURN_IGNORED sceneTable->RemoveFabric(fabricIndex);
    }
};

static ScenesClusterFabricDelegate gFabricDelegate;

CHIP_ERROR ScenesServer::Init()
{
    // Prevents re-initializing
    VerifyOrReturnError(!mIsInitialized, CHIP_ERROR_INCORRECT_STATE);

    ReturnErrorOnFailure(CommandHandlerInterfaceRegistry::Instance().RegisterCommandHandler(this));
    VerifyOrReturnError(AttributeAccessInterfaceRegistry::Instance().Register(this), CHIP_ERROR_INCORRECT_STATE);
    mGroupProvider = Credentials::GetGroupDataProvider();

    SceneTable * sceneTable = scenes::GetSceneTableImpl();
    ReturnErrorOnFailure(sceneTable->Init(Server::GetInstance().GetPersistentStorage(), CodegenDataModelProvider::Instance()));
    ReturnErrorOnFailure(Server::GetInstance().GetFabricTable().AddFabricDelegate(&gFabricDelegate));

    mIsInitialized = true;
    return CHIP_NO_ERROR;
}

void ScenesServer::Shutdown()
{
    Server::GetInstance().GetFabricTable().RemoveFabricDelegate(&gFabricDelegate);
    TEMPORARY_RETURN_IGNORED CommandHandlerInterfaceRegistry::Instance().UnregisterCommandHandler(this);
    AttributeAccessInterfaceRegistry::Instance().Unregister(this);

    mGroupProvider = nullptr;
    mIsInitialized = false;
}
#endif

CHIP_ERROR ScenesManagementCluster::StoreSceneParse(const FabricIndex & fabricIdx, const GroupId & groupID, const SceneId & sceneID)
{
    // Make the current fabric's SceneValid false before storing a scene
    MakeSceneInvalid(fabricIdx);

    // Get Scene Table Instance
    SceneTable * sceneTable = scenes::GetSceneTableImpl(mPath.mEndpointId, mSceneTableSize);

    // Verify Endpoint in group
    VerifyOrReturnError(nullptr != mGroupProvider, CHIP_ERROR_INTERNAL);
    if (0 != groupID && !mGroupProvider->HasEndpoint(fabricIdx, groupID, mPath.mEndpointId))
    {
        return CHIP_IM_GLOBAL_STATUS(InvalidCommand);
    }

    // Scene Table interface data
    SceneTableEntry scene(SceneStorageId(sceneID, groupID));

    VerifyOrReturnError(nullptr != sceneTable, CHIP_ERROR_INTERNAL);
    CHIP_ERROR err = sceneTable->GetSceneTableEntry(fabricIdx, scene.mStorageId, scene);
    if (CHIP_NO_ERROR != err && CHIP_ERROR_NOT_FOUND != err)
    {
        return err;
    }

    if (CHIP_ERROR_NOT_FOUND == err)
    {
        scene.mStorageData.SetName(CharSpan());
        scene.mStorageData.mSceneTransitionTimeMs = 0;
    }
    else
    {
        // Check if we still support scenes name in case an OTA changed that, if we don't, set name to empty
        if (!mFeatures.Has(ScenesManagement::Feature::kSceneNames))
        {
            scene.mStorageData.SetName(CharSpan());
        }
        scene.mStorageData.mExtensionFieldSets.Clear();
    }

    // Gets the EFS
    ReturnErrorOnFailure(sceneTable->SceneSaveEFS(scene));
    // Insert in Scene Table
    ReturnErrorOnFailure(sceneTable->SetSceneTableEntry(fabricIdx, scene));

    // Update SceneInfo Attribute
    return UpdateFabricSceneInfo(fabricIdx, MakeOptional(groupID), MakeOptional(sceneID), MakeOptional(static_cast<bool>(true)));
}

CHIP_ERROR ScenesManagementCluster::RecallSceneParse(const FabricIndex & fabricIdx, const GroupId & groupID,
                                                     const SceneId & sceneID,
                                                     const Optional<DataModel::Nullable<uint32_t>> & transitionTime)
{
    // Make SceneValid false for all fabrics before recalling a scene
    MakeSceneInvalidForAllFabrics();

    // Get Scene Table Instance
    SceneTable * sceneTable = scenes::GetSceneTableImpl(mPath.mEndpointId, mSceneTableSize);

    // Verify Endpoint in group
    VerifyOrReturnError(nullptr != mGroupProvider, CHIP_ERROR_INTERNAL);
    if (0 != groupID && !mGroupProvider->HasEndpoint(fabricIdx, groupID, mPath.mEndpointId))
    {
        return CHIP_IM_GLOBAL_STATUS(InvalidCommand);
    }

    // Scene Table interface data
    SceneTableEntry scene(SceneStorageId(sceneID, groupID));

    VerifyOrReturnError(nullptr != sceneTable, CHIP_ERROR_INTERNAL);
    ReturnErrorOnFailure(sceneTable->GetSceneTableEntry(fabricIdx, scene.mStorageId, scene));

    // Check for optional
    if (transitionTime.HasValue())
    {
        // Check for nullable
        if (!transitionTime.Value().IsNull())
        {
            scene.mStorageData.mSceneTransitionTimeMs = transitionTime.Value().Value();
        }
    }

    ReturnErrorOnFailure(sceneTable->SceneApplyEFS(scene));

    // Update FabricSceneInfo, at this point the scene is considered valid
    return UpdateFabricSceneInfo(fabricIdx, Optional<GroupId>(groupID), Optional<SceneId>(sceneID), Optional<bool>(true));
}

std::optional<DataModel::ActionReturnStatus> ScenesManagementCluster::InvokeCommand(const DataModel::InvokeRequest & request,
                                                                                    chip::TLV::TLVReader & input_arguments,
                                                                                    CommandHandler * handler)
{
    switch (request.path.mCommandId)
    {
    case AddScene::Id: {
        AddScene::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));
        handler->AddResponse(request.path, HandleAddScene(handler->GetAccessingFabricIndex(), request_data));
        return std::nullopt;
    }
    case ViewScene::Id: {
        ViewScene::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));
        handler->AddResponse(request.path, HandleViewScene(handler->GetAccessingFabricIndex(), request_data));
        return std::nullopt;
    }
    case RemoveScene::Id: {
        RemoveScene::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));
        handler->AddResponse(request.path, HandleRemoveScene(handler->GetAccessingFabricIndex(), request_data));
        return std::nullopt;
    }
    case RemoveAllScenes::Id: {
        RemoveAllScenes::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));
        handler->AddResponse(request.path, HandleRemoveAllScenes(handler->GetAccessingFabricIndex(), request_data));
        return std::nullopt;
    }
    case StoreScene::Id: {
        StoreScene::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));
        handler->AddResponse(request.path, HandleStoreScene(handler->GetAccessingFabricIndex(), request_data));
        return std::nullopt;
    }
    case RecallScene::Id: {
        RecallScene::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));
        return HandleRecallScene(handler->GetAccessingFabricIndex(), request_data);
    }
    case GetSceneMembership::Id: {
        GetSceneMembership::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));
        handler->AddResponse(request.path, HandleGetSceneMembership(handler->GetAccessingFabricIndex(), request_data));
        return std::nullopt;
    }
    case CopyScene::Id: {
        CopyScene::DecodableType request_data;
        ReturnErrorOnFailure(request_data.Decode(input_arguments, request.GetAccessingFabricIndex()));
        handler->AddResponse(request.path, HandleCopyScene(handler->GetAccessingFabricIndex(), request_data));
        return std::nullopt;
    }
    default:
        return Protocols::InteractionModel::Status::UnsupportedCommand;
    }
}

DataModel::ActionReturnStatus ScenesManagementCluster::ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                                     AttributeValueEncoder & encoder)
{
    switch (request.path.mAttributeId)
    {
    case ClusterRevision::Id:
        return encoder.Encode(ScenesManagement::kRevision);
    case FeatureMap::Id:
        return encoder.Encode(mFeatures);
    case SceneTableSize::Id:
        return encoder.Encode(mSceneTableSize);
    case ScenesManagement::Attributes::FabricSceneInfo::Id: {
        SceneTable * sceneTable = scenes::GetSceneTableImpl(request.path.mEndpointId, mSceneTableSize);
        return encoder.EncodeList([&, sceneTable](const auto & encoder) -> CHIP_ERROR {
            Span<SceneInfoStruct::Type> fabricSceneInfoSpan = mFabricSceneInfo.GetFabricSceneInfo();
            for (auto & info : fabricSceneInfoSpan)
            {
                // Update the SceneInfoStruct's Capacity in case it's capacity was limited by other fabrics
                TEMPORARY_RETURN_IGNORED sceneTable->GetRemainingCapacity(info.fabricIndex, info.remainingCapacity);
                ReturnErrorOnFailure(encoder.Encode(info));
            }
            return CHIP_NO_ERROR;
        });
    }
    default:
        return Protocols::InteractionModel::Status::UnsupportedAttribute;
    }
}

#if 0
using namespace chip;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::ScenesManagement;

void emberAfScenesManagementClusterServerInitCallback(EndpointId endpoint)
{
    // Initialize the FabricSceneInfo by getting the number of scenes and the remaining capacity for storing fabric scene data
    for (auto & info : chip::Server::GetInstance().GetFabricTable())
    {
        auto fabric = info.GetFabricIndex();
        TEMPORARY_RETURN_IGNORED UpdateFabricSceneInfo(endpoint, fabric, Optional<GroupId>(), Optional<SceneId>(),
                                                       Optional<bool>());
    }
}

void MatterScenesManagementClusterServerShutdownCallback(EndpointId endpoint)
{
    uint16_t endpointTableSize = 0;
    VerifyOrReturn(Status::Success == Attributes::SceneTableSize::Get(endpoint, &endpointTableSize));

    // Get Scene Table Instance
    SceneTable * sceneTable = scenes::GetSceneTableImpl(endpoint, endpointTableSize);
    TEMPORARY_RETURN_IGNORED sceneTable->RemoveEndpoint();
}

void MatterScenesManagementPluginServerInitCallback()
{
    CHIP_ERROR err = ScenesServer::Instance().Init();
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Zcl, "ScenesServer::Instance().Init() error: %" CHIP_ERROR_FORMAT, err.Format());
    }
}

void MatterScenesManagementPluginServerShutdownCallback()
{
    ScenesServer::Instance().Shutdown();
}
#endif

#if 0

void ScenesServer::GroupWillBeRemoved(FabricIndex aFabricIx, EndpointId aEndpointId, GroupId aGroupId)
{
    // Get Scene Table Instance
    SceneTable * sceneTable = scenes::GetSceneTableImpl(aEndpointId);
    VerifyOrReturn(nullptr != sceneTable);

    Structs::SceneInfoStruct::Type * sceneInfo = mFabricSceneInfo.GetSceneInfoStruct(aEndpointId, aFabricIx);
    chip::GroupId currentGroup                 = (nullptr != sceneInfo) ? sceneInfo->currentGroup : 0x0000;

    // If currentGroup is what is being removed, we can't possibly still have a valid scene,
    // because the scene we have (if any) will also be removed.
    if (aGroupId == currentGroup)
    {
        TEMPORARY_RETURN_IGNORED MakeSceneInvalid(aEndpointId, aFabricIx);
    }

    VerifyOrReturn(nullptr != mGroupProvider);
    if (0 != aGroupId && !mGroupProvider->HasEndpoint(aFabricIx, aGroupId, aEndpointId))
    {
        return;
    }

    TEMPORARY_RETURN_IGNORED sceneTable->DeleteAllScenesInGroup(aFabricIx, aGroupId);
}

void ScenesServer::MakeSceneInvalid(EndpointId aEndpointId, FabricIndex aFabricIx)
{
    TEMPORARY_RETURN_IGNORED UpdateFabricSceneInfo(aEndpointId, aFabricIx, Optional<GroupId>(), Optional<SceneId>(),
                                                   Optional<bool>(false));
}

void ScenesServer::MakeSceneInvalidForAllFabrics(EndpointId aEndpointId)
{
    for (auto & info : chip::Server::GetInstance().GetFabricTable())
    {
        TEMPORARY_RETURN_IGNORED MakeSceneInvalid(aEndpointId, info.GetFabricIndex());
    }
}

void ScenesServer::StoreCurrentScene(FabricIndex aFabricIx, EndpointId aEndpointId, GroupId aGroupId, SceneId aSceneId)
{
    TEMPORARY_RETURN_IGNORED StoreSceneParse(aFabricIx, aEndpointId, aGroupId, aSceneId, mGroupProvider);
}
void ScenesServer::RecallScene(FabricIndex aFabricIx, EndpointId aEndpointId, GroupId aGroupId, SceneId aSceneId)
{
    Optional<DataModel::Nullable<uint32_t>> transitionTime;

    TEMPORARY_RETURN_IGNORED RecallSceneParse(aFabricIx, aEndpointId, aGroupId, aSceneId, transitionTime, mGroupProvider);
}

bool ScenesServer::IsHandlerRegistered(EndpointId aEndpointId, scenes::SceneHandler * handler)
{
    SceneTable * sceneTable = scenes::GetSceneTableImpl(aEndpointId);
    return sceneTable->mHandlerList.Contains(handler);
}

void ScenesServer::RegisterSceneHandler(EndpointId aEndpointId, scenes::SceneHandler * handler)
{
    SceneTable * sceneTable = scenes::GetSceneTableImpl(aEndpointId);

    if (!IsHandlerRegistered(aEndpointId, handler))
    {
        sceneTable->RegisterHandler(handler);
    }
}

void ScenesServer::UnregisterSceneHandler(EndpointId aEndpointId, scenes::SceneHandler * handler)
{
    SceneTable * sceneTable = scenes::GetSceneTableImpl(aEndpointId);

    if (IsHandlerRegistered(aEndpointId, handler))
    {
        sceneTable->UnregisterHandler(handler);
    }
}

void ScenesServer::RemoveFabric(EndpointId aEndpointId, FabricIndex aFabricIndex)
{
    SceneTable * sceneTable = scenes::GetSceneTableImpl(aEndpointId);
    TEMPORARY_RETURN_IGNORED sceneTable->RemoveFabric(aFabricIndex);
    mFabricSceneInfo.ClearSceneInfoStruct(aEndpointId, aFabricIndex);
}
#endif

AddSceneResponse::Type ScenesManagementCluster::HandleAddScene(FabricIndex fabricIndex,
                                                               const ScenesManagement::Commands::AddScene::DecodableType & req)
{
    AddSceneResponse::Type response;
    response.groupID = req.groupID;
    response.sceneID = req.sceneID;

    // Get Scene Table Instance
    SceneTable * sceneTable = scenes::GetSceneTableImpl(mPath.mEndpointId, mSceneTableSize);

    // Verify the attributes are respecting constraints
    if (req.transitionTime > scenes::kScenesMaxTransitionTime || req.sceneName.size() > scenes::kSceneNameMaxLength ||
        req.sceneID == scenes::kUndefinedSceneId)
    {
        response.status = to_underlying(Protocols::InteractionModel::Status::ConstraintError);
        return response;
    }

    // Verify Endpoint in group
    if (nullptr == mGroupProvider)
    {
        response.status = to_underlying(Protocols::InteractionModel::Status::UnsupportedCommand);
        return response;
    }

    if (0 != req.groupID && !mGroupProvider->HasEndpoint(fabricIndex, req.groupID, mPath.mEndpointId))
    {
        response.status = to_underlying(Protocols::InteractionModel::Status::InvalidCommand);
        return response;
    }

    SceneData storageData(CharSpan(), req.transitionTime);
    if (mFeatures.Has(ScenesManagement::Feature::kSceneNames))
    {
        storageData.SetName(req.sceneName);
    }

    auto fieldSetIter = req.extensionFieldSetStructs.begin();
    uint8_t EFSCount  = 0;
    // Goes through all EFS in command
    while (fieldSetIter.Next() && EFSCount < scenes::kMaxClustersPerScene)
    {
        scenes::ExtensionFieldSet tempEFS;
        tempEFS.mID = fieldSetIter.GetValue().clusterID;

        MutableByteSpan buff_span(tempEFS.mBytesBuffer);

        // Check if a handler is registered for the EFS's cluster
        for (auto & handler : sceneTable->mHandlerList)
        {
            if (handler.SupportsCluster(mPath.mEndpointId, tempEFS.mID))
            {
                VerifyOrReturnValue(
                    ValidateSuccess(handler.SerializeAdd(mPath.mEndpointId, fieldSetIter.GetValue(), buff_span), response),
                    response);
                break;
            }
        }

        static_assert(sizeof(tempEFS.mBytesBuffer) <= UINT8_MAX, "Serialized EFS number of bytes must fit in a uint8");
        tempEFS.mUsedBytes = static_cast<uint8_t>(buff_span.size());

        if (!tempEFS.IsEmpty())
        {
            TEMPORARY_RETURN_IGNORED storageData.mExtensionFieldSets.InsertFieldSet(tempEFS);
        }
    }
    VerifyOrReturnValue(ValidateSuccess(fieldSetIter.GetStatus(), response), response);

    // Create scene from data and ID
    SceneTableEntry scene(SceneStorageId(req.sceneID, req.groupID), storageData);

    // Get Capacity
    if (nullptr == sceneTable)
    {
        response.status = to_underlying(Status::Failure);
        return response;
    }

    uint8_t capacity = 0;
    VerifyOrReturnValue(ValidateSuccess(sceneTable->GetRemainingCapacity(fabricIndex, capacity), response), response);

    if (0 == capacity)
    {
        response.status = to_underlying(Protocols::InteractionModel::Status::ResourceExhausted);
        return response;
    }

    //  Insert in table
    VerifyOrReturnValue(ValidateSuccess(sceneTable->SetSceneTableEntry(fabricIndex, scene), response), response);

    // Update FabricSceneInfo
    VerifyOrReturnValue(
        ValidateSuccess(UpdateFabricSceneInfo(fabricIndex, Optional<GroupId>(), Optional<SceneId>(), Optional<bool>()), response),
        response);

    // Write response
    response.status = to_underlying(Protocols::InteractionModel::Status::Success);
    return response;
}

ViewSceneResponse::Type ScenesManagementCluster::HandleViewScene(FabricIndex fabricIndex,
                                                                 const ScenesManagement::Commands::ViewScene::DecodableType & req)
{
    MATTER_TRACE_SCOPE("ViewScene", "Scenes");

    ViewSceneResponse::Type response;
    response.groupID = req.groupID;
    response.sceneID = req.sceneID;

    // Get Scene Table Instance
    SceneTable * sceneTable = scenes::GetSceneTableImpl(mPath.mEndpointId, mSceneTableSize);

    // Verify the attributes are respecting constraints
    if (req.sceneID == scenes::kUndefinedSceneId)
    {
        response.status = to_underlying(Status::ConstraintError);
        return response;
    }

    // Verify Endpoint in group
    if (nullptr == mGroupProvider)
    {
        response.status = to_underlying(Status::Failure);
        return response;
    }

    if (0 != req.groupID && !mGroupProvider->HasEndpoint(fabricIndex, req.groupID, mPath.mEndpointId))
    {
        response.status = to_underlying(Protocols::InteractionModel::Status::InvalidCommand);
        return response;
    }

    SceneTableEntry scene;

    //  Gets the scene from the table
    VerifyOrReturnValue(
        ValidateSuccess(sceneTable->GetSceneTableEntry(fabricIndex, SceneStorageId(req.sceneID, req.groupID), scene), response),
        response);

    // Response Extension Field Sets buffer
    ExtensionFieldSetStruct::Type responseEFSBuffer[scenes::kMaxClustersPerScene];
    uint8_t deserializedEFSCount = 0;

    // Adds extension field sets to the scene
    for (uint8_t i = 0; i < scene.mStorageData.mExtensionFieldSets.GetFieldSetCount(); i++)
    {
        // gets data from the field in the scene
        ExtensionFieldSet tempField;
        TEMPORARY_RETURN_IGNORED scene.mStorageData.mExtensionFieldSets.GetFieldSetAtPosition(tempField, i);
        ByteSpan efsSpan(tempField.mBytesBuffer, tempField.mUsedBytes);

        // This should only find one handle per cluster
        for (auto & handler : sceneTable->mHandlerList)
        {
            if (handler.SupportsCluster(mPath.mEndpointId, tempField.mID))
            {
                VerifyOrReturnValue(
                    ValidateSuccess(handler.Deserialize(mPath.mEndpointId, tempField.mID, efsSpan, responseEFSBuffer[i]), response),
                    response);
                deserializedEFSCount++;
                break;
            }
        }
    }

    response.status = to_underlying(Protocols::InteractionModel::Status::Success);
    response.transitionTime.SetValue(scene.mStorageData.mSceneTransitionTimeMs);

    response.sceneName.SetValue(CharSpan(scene.mStorageData.mName, scene.mStorageData.mNameLength));
    Span<ExtensionFieldSetStruct::Type> responseEFSSpan(responseEFSBuffer, deserializedEFSCount);
    response.extensionFieldSetStructs.SetValue(responseEFSSpan);

    return response;
}

RemoveSceneResponse::Type
ScenesManagementCluster::HandleRemoveScene(FabricIndex fabricIndex,
                                           const ScenesManagement::Commands::RemoveScene::DecodableType & req)
{
    MATTER_TRACE_SCOPE("RemoveScene", "Scenes");
    // Write response
    RemoveSceneResponse::Type response;

    response.groupID = req.groupID;
    response.sceneID = req.sceneID;

    // Get Scene Table Instance
    SceneTable * sceneTable = scenes::GetSceneTableImpl(mPath.mEndpointId, mSceneTableSize);

    // Verify the attributes are respecting constraints
    if (req.sceneID == scenes::kUndefinedSceneId)
    {
        response.status = to_underlying(Status::ConstraintError);
        return response;
    }

    // Scene Table interface data
    SceneTableEntry scene(SceneStorageId(req.sceneID, req.groupID));

    // Verify Endpoint in group
    if (nullptr != mGroupProvider)
    {
        response.status = to_underlying(Status::Failure);
        return response;
    }
    if (0 != req.groupID && !mGroupProvider->HasEndpoint(fabricIndex, req.groupID, mPath.mEndpointId))
    {
        response.status = to_underlying(Status::InvalidCommand);
        return response;
    }

    //  Gets the scene from the table
    VerifyOrReturnValue(ValidateSuccess(sceneTable->GetSceneTableEntry(fabricIndex, scene.mStorageId, scene), response), response);

    // Remove the scene from the scene table
    VerifyOrReturnValue(ValidateSuccess(sceneTable->RemoveSceneTableEntry(fabricIndex, scene.mStorageId), response), response);

    // Update SceneInfoStruct Attributes
    SceneInfoStruct::Type * sceneInfo = GetSceneInfoStruct(fabricIndex);
    Optional<bool> sceneValid;
    if (nullptr != sceneInfo && req.groupID == sceneInfo->currentGroup && req.sceneID == sceneInfo->currentScene)
    {
        sceneValid.Emplace(false);
    }

    VerifyOrReturnValue(
        ValidateSuccess(UpdateFabricSceneInfo(fabricIndex, Optional<GroupId>(), Optional<SceneId>(), sceneValid), response),
        response);

    response.status = to_underlying(Protocols::InteractionModel::Status::Success);
    return response;
}

RemoveAllScenesResponse::Type
ScenesManagementCluster::HandleRemoveAllScenes(FabricIndex fabricIndex,
                                               const ScenesManagement::Commands::RemoveAllScenes::DecodableType & req)
{
    MATTER_TRACE_SCOPE("RemoveAllScenes", "Scenes");
    // Response data
    RemoveAllScenesResponse::Type response;
    response.groupID = req.groupID;

    // Get Scene Table Instance
    SceneTable * sceneTable = scenes::GetSceneTableImpl(mPath.mEndpointId, mSceneTableSize);

    // Verify Endpoint in group
    if (nullptr != mGroupProvider)
    {
        response.status = to_underlying(Status::Failure);
        return response;
    }
    if (0 != req.groupID && !mGroupProvider->HasEndpoint(fabricIndex, req.groupID, mPath.mEndpointId))
    {
        response.status = to_underlying(Protocols::InteractionModel::Status::InvalidCommand);
        return response;
    }

    VerifyOrReturnValue(ValidateSuccess(sceneTable->DeleteAllScenesInGroup(fabricIndex, req.groupID), response), response);

    // Update Attributes
    SceneInfoStruct::Type * sceneInfo = GetSceneInfoStruct(fabricIndex);

    Optional<bool> sceneValid;
    if (nullptr != sceneInfo && req.groupID == sceneInfo->currentGroup)
    {
        sceneValid.Emplace(false);
    }

    VerifyOrReturnValue(
        ValidateSuccess(UpdateFabricSceneInfo(fabricIndex, Optional<GroupId>(), Optional<SceneId>(), sceneValid), response),
        response);

    response.status = to_underlying(Protocols::InteractionModel::Status::Success);
    return response;
}

ScenesManagement::Commands::StoreSceneResponse::Type
ScenesManagementCluster::HandleStoreScene(FabricIndex fabricIndex,
                                          const ScenesManagement::Commands::StoreScene::DecodableType & req)
{
    MATTER_TRACE_SCOPE("StoreScene", "Scenes");
    StoreSceneResponse::Type response;
    response.groupID = req.groupID;
    response.sceneID = req.sceneID;

    // Verify the attributes are respecting constraints
    if (req.sceneID == scenes::kUndefinedSceneId)
    {
        response.status = to_underlying(Protocols::InteractionModel::Status::ConstraintError);
        return response;
    }

    VerifyOrReturnValue(ValidateSuccess(StoreSceneParse(fabricIndex, req.groupID, req.sceneID), response), response);

    response.status = to_underlying(Protocols::InteractionModel::Status::Success);
    return response;
}

Protocols::InteractionModel::Status
ScenesManagementCluster::HandleRecallScene(FabricIndex fabricIndex,
                                           const ScenesManagement::Commands::RecallScene::DecodableType & req)
{
    MATTER_TRACE_SCOPE("RecallScene", "Scenes");

    // Verify the attributes are respecting constraints
    VerifyOrReturnError(req.sceneID != scenes::kUndefinedSceneId, Status::ConstraintError);

    CHIP_ERROR err = RecallSceneParse(fabricIndex, req.groupID, req.sceneID, req.transitionTime);

    if (CHIP_NO_ERROR == err)
    {
        return Protocols::InteractionModel::Status::Success;
    }

    if (CHIP_ERROR_NOT_FOUND == err)
    {
        // TODO : implement proper mapping between CHIP_ERROR and IM Status
        return Protocols::InteractionModel::Status::NotFound;
    }

    return StatusIB(err).mStatus;
}

ScenesManagement::Commands::GetSceneMembershipResponse::Type
ScenesManagementCluster::HandleGetSceneMembership(FabricIndex fabricIndex,
                                                  const ScenesManagement::Commands::GetSceneMembership::DecodableType & req)
{
    MATTER_TRACE_SCOPE("GetSceneMembership", "Scenes");
    GetSceneMembershipResponse::Type response;
    response.groupID = req.groupID;

    // Verify Endpoint in group
    if (nullptr != mGroupProvider)
    {
        response.status = to_underlying(Status::Failure);
        return response;
    }

    if (0 != req.groupID && !mGroupProvider->HasEndpoint(fabricIndex, req.groupID, mPath.mEndpointId))
    {
        response.status = to_underlying(Protocols::InteractionModel::Status::InvalidCommand);
        return response;
    }

    uint8_t capacity        = 0;
    SceneTable * sceneTable = scenes::GetSceneTableImpl(mPath.mEndpointId, mSceneTableSize);

    // Get Capacity
    VerifyOrReturnValue(ValidateSuccess(sceneTable->GetRemainingCapacity(fabricIndex, capacity), response), response);
    response.capacity.SetNonNull(capacity);

    // populate scene list
    SceneId scenesInGroup[scenes::kMaxScenesPerFabric];
    auto sceneList = Span<SceneId>(scenesInGroup);
    VerifyOrReturnError(ValidateSuccess(sceneTable->GetAllSceneIdsInGroup(fabricIndex, req.groupID, sceneList), response),
                        response);

    response.sceneList.SetValue(sceneList);

    // Write response
    response.status = to_underlying(Protocols::InteractionModel::Status::Success);
    return response;
}

ScenesManagement::Commands::CopySceneResponse::Type
ScenesManagementCluster::HandleCopyScene(FabricIndex fabricIndex, const ScenesManagement::Commands::CopyScene::DecodableType & req)
{
    MATTER_TRACE_SCOPE("CopyScene", "Scenes");
    CopySceneResponse::Type response;
    response.groupIdentifierFrom = req.groupIdentifierFrom;
    response.sceneIdentifierFrom = req.sceneIdentifierFrom;

    // Verify the attributes are respecting constraints
    if (req.sceneIdentifierFrom == scenes::kUndefinedSceneId || req.sceneIdentifierTo == scenes::kUndefinedSceneId)
    {
        response.status = to_underlying(Protocols::InteractionModel::Status::ResourceExhausted);
        return response;
    }

    // Verify Endpoint in group
    if (nullptr != mGroupProvider)
    {
        response.status = to_underlying(Status::Failure);
        return response;
    }

    if ((0 != req.groupIdentifierFrom && !mGroupProvider->HasEndpoint(fabricIndex, req.groupIdentifierFrom, mPath.mEndpointId)) ||
        (0 != req.groupIdentifierTo && !mGroupProvider->HasEndpoint(fabricIndex, req.groupIdentifierTo, mPath.mEndpointId)))
    {
        response.status = to_underlying(Protocols::InteractionModel::Status::InvalidCommand);
        return response;
    }

    SceneTable * sceneTable = scenes::GetSceneTableImpl(mPath.mEndpointId, mSceneTableSize);
    uint8_t capacity        = 0;

    VerifyOrReturnValue(ValidateSuccess(sceneTable->GetRemainingCapacity(fabricIndex, capacity), response), response);

    if (0 == capacity)
    {
        response.status = to_underlying(Protocols::InteractionModel::Status::ResourceExhausted);
        return response;
    }

    // Checks if we copy a single scene or all of them
    if (req.mode.GetField(app::Clusters::ScenesManagement::CopyModeBitmap::kCopyAllScenes))
    {
        // Scene Table interface data
        SceneId scenesInGroup[scenes::kMaxScenesPerFabric];
        Span<SceneId> sceneList = Span<SceneId>(scenesInGroup);

        // populate scene list
        VerifyOrReturnValue(
            ValidateSuccess(sceneTable->GetAllSceneIdsInGroup(fabricIndex, req.groupIdentifierFrom, sceneList), response),
            response);

        for (auto & sceneId : sceneList)
        {
            SceneTableEntry scene(SceneStorageId(sceneId, req.groupIdentifierFrom));
            //  Insert in table
            VerifyOrReturnValue(ValidateSuccess(sceneTable->GetSceneTableEntry(fabricIndex, scene.mStorageId, scene), response),
                                response);

            scene.mStorageId = SceneStorageId(sceneId, req.groupIdentifierTo);

            VerifyOrReturnValue(ValidateSuccess(sceneTable->SetSceneTableEntry(fabricIndex, scene), response), response);

            // Update SceneInfoStruct Attributes after each insert in case we hit max capacity in the middle of the loop
            VerifyOrReturnValue(ValidateSuccess(UpdateFabricSceneInfo(fabricIndex, Optional<GroupId>(), Optional<SceneId>(),
                                                                      Optional<bool>() /* = sceneValid*/),
                                                response),
                                response);
        }

        response.status = to_underlying(Protocols::InteractionModel::Status::Success);
        return response;
    }

    SceneTableEntry scene(SceneStorageId(req.sceneIdentifierFrom, req.groupIdentifierFrom));
    VerifyOrReturnValue(ValidateSuccess(sceneTable->GetSceneTableEntry(fabricIndex, scene.mStorageId, scene), response), response);

    scene.mStorageId = SceneStorageId(req.sceneIdentifierTo, req.groupIdentifierTo);

    VerifyOrReturnValue(ValidateSuccess(sceneTable->SetSceneTableEntry(fabricIndex, scene), response), response);

    // Update Attributes
    VerifyOrReturnValue(
        ValidateSuccess(UpdateFabricSceneInfo(fabricIndex, Optional<GroupId>(), Optional<SceneId>(), Optional<bool>()), response),
        response);

    response.status = to_underlying(Protocols::InteractionModel::Status::Success);
    return response;
}

} // namespace chip::app::Clusters
