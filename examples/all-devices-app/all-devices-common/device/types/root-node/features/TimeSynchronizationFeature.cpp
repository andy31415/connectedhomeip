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
#include <device/types/root-node/features/TimeSynchronizationFeature.h>

#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace chip::app::Clusters;

namespace chip {
namespace app {

CHIP_ERROR TimeSynchronizationFeature::RegisterFeatureClusters(EndpointId endpointId, CodeDrivenDataModelProvider & provider,
                                                               RootNode::Context & rootContext,
                                                               Clusters::GeneralCommissioningCluster &)
{
    if (!mContext.enabled)
    {
        return CHIP_NO_ERROR;
    }

    BitFlags<TimeSynchronization::Feature> features;
    features.Set(TimeSynchronization::Feature::kTimeZone);
    features.Set(TimeSynchronization::Feature::kNTPClient);
#if TIME_SYNC_ENABLE_TSC_FEATURE
    features.Set(TimeSynchronization::Feature::kTimeSyncClient);
#endif // TIME_SYNC_ENABLE_TSC_FEATURE

    TimeSynchronizationCluster::OptionalAttributeSet optionalAttributes;
    optionalAttributes.Set<TimeSynchronization::Attributes::TimeSource::Id>();

    TimeSynchronizationCluster::StartupConfiguration startupConfig{
        .delegate = mContext.delegate != nullptr ? mContext.delegate : &mDefaultDelegate,
    };

    TimeSynchronizationCluster::Context clusterContext{
        .fabricTable            = rootContext.fabricTable,
        .caseSessionManager     = mContext.caseSessionManager,
        .platformManager        = rootContext.platformManager,
        .interactionModelEngine = mContext.interactionModelEngine,
    };

    mTimeSyncCluster.Create(endpointId, features, optionalAttributes, startupConfig, clusterContext);
    return provider.AddCluster(mTimeSyncCluster.Registration());
}

void TimeSynchronizationFeature::UnregisterFeatureClusters(CodeDrivenDataModelProvider & provider)
{
    if (mTimeSyncCluster.IsConstructed())
    {
        LogErrorOnFailure(provider.RemoveCluster(&mTimeSyncCluster.Cluster()));
        mTimeSyncCluster.Destroy();
    }
}

} // namespace app
} // namespace chip
