/**
 *    Copyright (c) 2025 Project CHIP Authors
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
#include <app/clusters/scenes-server/CodegenIntegration.h>

#include <app/clusters/scenes-server/SceneTable.h>
#include <app/clusters/scenes-server/SceneTableImpl.h>
#include <app/static-cluster-config/ScenesManagement.h>
#include <data-model-providers/codegen/ClusterIntegration.h>
#include <data-model-providers/codegen/CodegenDataModelProvider.h>

using SceneTable = chip::scenes::SceneTable<chip::scenes::ExtensionFieldSetsImpl>;

namespace chip::app::Clusters::ScenesManagement {

namespace {

constexpr size_t kScenesmanagementFixedClusterCount = StaticApplicationConfig::kFixedClusterConfig.size();
constexpr size_t kScenesmanagementMaxClusterCount   = kScenesmanagementFixedClusterCount + CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT;

LazyRegisteredServerCluster<ScenesManagementCluster> gServers[kScenesmanagementMaxClusterCount];

}

ScenesServer & ScenesServer::Instance()
{
    static ScenesServer gInstance;
    return gInstance;
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

// TODO: figure these out!!!

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

void ScenesServer::RemoveFabric(EndpointId aEndpointId, FabricIndex aFabricIndex)
{
    SceneTable * sceneTable = scenes::GetSceneTableImpl(aEndpointId);
    TEMPORARY_RETURN_IGNORED sceneTable->RemoveFabric(aFabricIndex);
    mFabricSceneInfo.ClearSceneInfoStruct(aEndpointId, aFabricIndex);
}

} // namespace chip::app::Clusters::ScenesManagement
