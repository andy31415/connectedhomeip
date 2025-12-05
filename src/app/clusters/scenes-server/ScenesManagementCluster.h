/**
 *
 *    Copyright (c) 2023 Project CHIP Authors
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

#include <app/clusters/scenes-server/SceneTableImpl.h>
#include <app/server-cluster/DefaultServerCluster.h>
#include <clusters/ScenesManagement/AttributeIds.h>
#include <clusters/ScenesManagement/ClusterId.h>
#include <clusters/ScenesManagement/Structs.h>
#include <credentials/GroupDataProvider.h>
#include <limits>

namespace chip::app::Clusters {

class ScenesManagementCluster : public DefaultServerCluster
{
public:
    static constexpr uint8_t kScenesServerMaxFabricCount = CHIP_CONFIG_MAX_FABRICS;
    static constexpr SceneId kGlobalSceneId              = 0x00;
    static constexpr GroupId kGlobalSceneGroupId         = 0x0000;

    using SceneInfoStructType = ScenesManagement::Structs::SceneInfoStruct::Type;

    class FabricSceneInfo
    {
    public:
        Span<SceneInfoStructType> GetFabricSceneInfo() { return { mSceneInfoStructs, mSceneInfoStructsCount }; }

        /// Gets the SceneInfoStruct for a specific fabric.
        ///
        /// returns nullptr if not found
        SceneInfoStructType * GetSceneInfoStruct(FabricIndex fabric);

        /// Sets the SceneInfoStruct for a specific fabric for a specific endpoint.
        CHIP_ERROR SetSceneInfoStruct(FabricIndex fabric, const SceneInfoStructType & sceneInfoStruct);

        /// Clears the SceneInfoStruct associated to a fabric and compresses the array to leave uninitialised structs at the end.
        void ClearSceneInfoStruct(FabricIndex fabric);

    private:
        static_assert(kScenesServerMaxFabricCount <= std::numeric_limits<uint8_t>::max());

        /// Returns the SceneInfoStruct associated to a fabric
        ///
        /// @param[in] fabric target fabric index
        /// @param[out] index index of the corresponding SceneInfoStruct if found, otherwise the index value will be invalid and
        /// should not be used. This is safe to store in a uint8_t because the index is guaranteed to be smaller than
        /// CHIP_CONFIG_MAX_FABRICS.
        ///
        /// @return CHIP_NO_ERROR or CHIP_ERROR_NOT_FOUND, CHIP_ERROR_INVALID_ARGUMENT if invalid fabric or endpoint
        CHIP_ERROR FindSceneInfoStructIndex(FabricIndex fabric, uint8_t & index);
        SceneInfoStructType mSceneInfoStructs[kScenesServerMaxFabricCount];
        uint8_t mSceneInfoStructsCount = 0;
    };

    /// Injected dependencies of this cluster
    struct Context
    {
        Credentials::GroupDataProvider * groupDataProvider;
        const BitMask<ScenesManagement::Feature> features;
        uint16_t sceneTableSize;
        const bool supportsCopyScene;
    };

    ScenesManagementCluster(EndpointId endpointId, const Context & context) :
        DefaultServerCluster({ endpointId, ScenesManagement::Id }), mFeatures(context.features),
        mSceneTableSize(context.sceneTableSize), mSupportCopyScenes(context.supportsCopyScene),
        mGroupProvider(context.groupDataProvider)
    {}

    // ServerClusterInterface/DefaultServerCluster implementation
    CHIP_ERROR Attributes(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder) override;
    DataModel::ActionReturnStatus ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                AttributeValueEncoder & encoder) override;
    CHIP_ERROR AcceptedCommands(const ConcreteClusterPath & path,
                                ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> & builder) override;
    CHIP_ERROR GeneratedCommands(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<CommandId> & builder) override;
    std::optional<DataModel::ActionReturnStatus> InvokeCommand(const DataModel::InvokeRequest & request,
                                                               chip::TLV::TLVReader & input_arguments,
                                                               CommandHandler * handler) override;

    // TODO: which of these should be kept???

    // SceneInfoStruct Accessors
    SceneInfoStructType * GetSceneInfoStruct(FabricIndex fabric);
    CHIP_ERROR SetSceneInfoStruct(FabricIndex fabric, SceneInfoStructType & sceneInfoStruct);

    // Callbacks
    void GroupWillBeRemoved(FabricIndex aFabricIx, GroupId aGroupId);
    void MakeSceneInvalid(FabricIndex aFabricIx);
    void MakeSceneInvalidForAllFabrics();
    void StoreCurrentScene(FabricIndex aFabricIx, GroupId aGroupId, SceneId aSceneId);
    void RecallScene(FabricIndex aFabricIx, GroupId aGroupId, SceneId aSceneId);

    // Handlers for extension field sets
    bool IsHandlerRegistered(scenes::SceneHandler * handler);
    void RegisterSceneHandler(scenes::SceneHandler * handler);
    void UnregisterSceneHandler(scenes::SceneHandler * handler);

    // Fabric
    void RemoveFabric(FabricIndex aFabricIndex);

private:
    const BitMask<ScenesManagement::Feature> mFeatures;
    const uint16_t mSceneTableSize;
    bool mSupportCopyScenes;
    Credentials::GroupDataProvider * mGroupProvider = nullptr;
    FabricSceneInfo mFabricSceneInfo;

    //  Command handlers
    std::optional<DataModel::ActionReturnStatus> HandleAddScene(CommandHandler * handler,
                                                                const ScenesManagement::Commands::AddScene::DecodableType & req);
    std::optional<DataModel::ActionReturnStatus> HandleViewScene(CommandHandler * handler,
                                                                 const ScenesManagement::Commands::ViewScene::DecodableType & req);
    std::optional<DataModel::ActionReturnStatus>
    HandleRemoveScene(CommandHandler * handler, const ScenesManagement::Commands::RemoveScene::DecodableType & req);
    std::optional<DataModel::ActionReturnStatus>
    HandleRemoveAllScenes(CommandHandler * handler, const ScenesManagement::Commands::RemoveAllScenes::DecodableType & req);
    std::optional<DataModel::ActionReturnStatus>
    HandleStoreScene(CommandHandler * handler, const ScenesManagement::Commands::StoreScene::DecodableType & req);
    Protocols::InteractionModel::Status HandleRecallScene(const ScenesManagement::Commands::RecallScene::DecodableType & req);
    std::optional<DataModel::ActionReturnStatus>
    HandleGetSceneMembership(CommandHandler * handler, const ScenesManagement::Commands::GetSceneMembership::DecodableType & req);
    std::optional<DataModel::ActionReturnStatus> HandleCopyScene(CommandHandler * handler,
                                                                 const ScenesManagement::Commands::CopyScene::DecodableType & req);
};

} // namespace chip::app::Clusters
