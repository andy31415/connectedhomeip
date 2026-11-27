/*
 *
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
#include <device/types/snapshot-camera/SnapshotCamera.h>

#include <devices/Types.h>
#include <lib/support/CodeUtils.h>

using namespace chip::app::Clusters::CameraAvStreamManagement;

namespace chip {
namespace app {

SnapshotCamera::SnapshotCamera(const Context & context) : SnapshotCamera(context, Config{}) {}

SnapshotCamera::SnapshotCamera(const Context & context, const Config & config) :
    SingleEndpoint(Span<const DataModel::DeviceTypeEntry>(&Device::Type::kSnapshotCamera, 1)), mContext(context), mConfig(config)
{}

CHIP_ERROR SnapshotCamera::Register(chip::EndpointId endpoint, CodeDrivenDataModelProvider & provider,
                                    EndpointComposition composition)
{
    VerifyOrReturnError(mEndpointId == kInvalidEndpointId, CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(mConfig.features.Has(Feature::kSnapshot) && !mConfig.features.HasAny(Feature::kVideo, Feature::kAudio),
                        CHIP_ERROR_INVALID_ARGUMENT);

    DeviceRegistrationTransaction transaction(*this, provider);

    ReturnErrorOnFailure(RegisterDescriptor(endpoint, provider, composition));

    mCameraAVStreamMgmtCluster.Create(CameraAVStreamManagementCluster::InitArguments{
        .delegate                = mContext.avsmDelegate,
        .endpointId              = endpoint,
        .features                = mConfig.features,
        .optionalAttrs           = mConfig.optionalAttrs,
        .nightVisionUsesInfrared = mConfig.nightVisionUsesInfrared,
        .maxContentBufferSize    = mConfig.maxContentBufferSize,
        .snapshotCapabilities    = mConfig.snapshotCapabilities,
        .maxNetworkBandwidth     = mConfig.maxNetworkBandwidth,
        .supportedStreamUsages   = mConfig.supportedStreamUsages,
        .streamUsagePriorities   = mConfig.streamUsagePriorities,
        .defaultAttributeValues  = mConfig.defaultAttributeValues,
    });
    ReturnErrorOnFailure(provider.AddCluster(mCameraAVStreamMgmtCluster.Registration()));

    ReturnErrorOnFailure(RegisterAdditionalClusters(endpoint, provider));

    ReturnErrorOnFailure(provider.AddEndpoint(mEndpointRegistration));
    transaction.Commit();
    return CHIP_NO_ERROR;
}

void SnapshotCamera::Unregister(CodeDrivenDataModelProvider & provider)
{
    UnregisterDescriptor(provider);
    UnregisterAdditionalClusters(provider);

    if (mCameraAVStreamMgmtCluster.IsConstructed())
    {
        LogErrorOnFailure(provider.RemoveCluster(&mCameraAVStreamMgmtCluster.Cluster()));
        mCameraAVStreamMgmtCluster.Destroy();
    }
}

Clusters::CameraAvStreamManagement::CameraAVStreamManagementCluster & SnapshotCamera::CameraAVStreamManagementCluster()
{
    VerifyOrDie(mCameraAVStreamMgmtCluster.IsConstructed());
    return mCameraAVStreamMgmtCluster.Cluster();
}

} // namespace app
} // namespace chip
