/*
 *
 *    Copyright (c) 2026 Project CHIP Authors
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

#include <array>

#include <app/AppConfig.h>
#include <app/CASESessionManager.h>
#include <app/InteractionModelEngine.h>
#include <app/clusters/time-synchronization-server/DefaultTimeSyncDelegate.h>
#include <app/clusters/time-synchronization-server/TimeSynchronizationCluster.h>
#include <app/clusters/time-synchronization-server/time-synchronization-delegate.h>
#include <app/server-cluster/ServerClusterInterfaceRegistry.h>
#include <data-model-providers/codedriven/CodeDrivenDataModelProvider.h>
#include <device/types/root-node/RootNode.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>

namespace chip {
namespace app {

/// Feature policy that adds the Time Synchronization cluster to the root endpoint.
///
/// Uses `Context::delegate` when non-null, falling back to an owned
/// `DefaultTimeSyncDelegate`. `Feature::kTimeSyncClient` is gated on
/// `TIME_SYNC_ENABLE_TSC_FEATURE` for platforms that disable `ReadClient`.
/// When `Context::enabled` is false, registration is a no-op.
class TimeSynchronizationFeature
{
public:
    struct Context
    {
        bool enabled = true;
        CASESessionManager & caseSessionManager;
        InteractionModelEngine & interactionModelEngine;
        Clusters::TimeSynchronization::Delegate * delegate = nullptr;
    };

    explicit TimeSynchronizationFeature(const Context & context) : mContext(context) {}

    static constexpr std::array<DataModel::DeviceTypeEntry, 0> kExtraDeviceTypes{};
    static constexpr std::array<ClusterId, 0> kExtraClientClusters{};

    CHIP_ERROR RegisterFeatureClusters(EndpointId endpointId, CodeDrivenDataModelProvider & provider,
                                       RootNode::Context & rootContext,
                                       Clusters::GeneralCommissioningCluster & generalCommissioning);

    void UnregisterFeatureClusters(CodeDrivenDataModelProvider & provider);

    bool IsEnabled() const { return mContext.enabled; }
    Clusters::TimeSynchronizationCluster & Cluster() { return mTimeSyncCluster.Cluster(); }

private:
    Context mContext;
    Clusters::TimeSynchronization::DefaultTimeSyncDelegate mDefaultDelegate;
    LazyRegisteredServerCluster<Clusters::TimeSynchronizationCluster> mTimeSyncCluster;
};

} // namespace app
} // namespace chip
