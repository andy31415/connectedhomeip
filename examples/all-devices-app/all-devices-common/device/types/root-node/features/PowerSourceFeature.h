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
#include <cstdint>

#include <app/clusters/power-source-server/PowerSourceCluster.h>
#include <app/server-cluster/OptionalAttributeSet.h>
#include <app/server-cluster/ServerClusterInterfaceRegistry.h>
#include <clusters/PowerSource/AttributeIds.h>
#include <clusters/PowerSource/Enums.h>
#include <data-model-providers/codedriven/CodeDrivenDataModelProvider.h>
#include <device/types/root-node/RootNode.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/Span.h>

namespace chip {
namespace app {

/// Feature policy that adds a wired Power Source cluster to the root endpoint.
///
/// Advertises wired mains/DC power for the root node without adding an extra
/// device type or client cluster. When `Context::enabled` is false,
/// registration is a no-op so compositions can opt in at runtime.
class PowerSourceFeature
{
public:
    using PowerSourceCluster =
        Clusters::PowerSourceCluster<BitFlags<Clusters::PowerSource::Feature>(Clusters::PowerSource::Feature::kWired).Raw(),
                                     OptionalAttributeSet<Clusters::PowerSource::Attributes::WiredCurrentType::Id>::All()>;

    struct Context
    {
        bool enabled = true;
        /// Must remain valid for the lifetime of the feature and registered cluster.
        CharSpan description                                         = {};
        Clusters::PowerSource::WiredCurrentTypeEnum wiredCurrentType = Clusters::PowerSource::WiredCurrentTypeEnum::kAc;
        uint8_t order                                                = 0;
    };

    explicit PowerSourceFeature(const Context & context) : mContext(context) {}

    static constexpr std::array<DataModel::DeviceTypeEntry, 0> kExtraDeviceTypes{};
    static constexpr std::array<ClusterId, 0> kExtraClientClusters{};

    CHIP_ERROR RegisterFeatureClusters(EndpointId endpointId, CodeDrivenDataModelProvider & provider,
                                       RootNode::Context & rootContext,
                                       Clusters::GeneralCommissioningCluster & generalCommissioning);

    void UnregisterFeatureClusters(CodeDrivenDataModelProvider & provider);

    bool IsEnabled() const { return mContext.enabled; }
    PowerSourceCluster & Cluster() { return mPowerSourceCluster.Cluster(); }

private:
    Context mContext;
    EndpointId mEndpointList[1] = { kRootEndpointId };
    LazyRegisteredServerCluster<PowerSourceCluster> mPowerSourceCluster;
};

} // namespace app
} // namespace chip
