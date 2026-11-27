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
#include <device/types/camera/Camera.h>

#include <clusters/WebRTCTransportRequestor/ClusterId.h>
#include <devices/Types.h>
#include <lib/support/CodeUtils.h>

using namespace chip::app::Clusters;
using namespace chip::app::Clusters::CameraAvStreamManagement;

namespace chip {
namespace app {

namespace {
constexpr ClusterId kMandatoryClientClusters[] = { WebRTCTransportRequestor::Id };
} // namespace

Camera::Camera(const Context & context) : Camera(context, Config{}) {}

Camera::Camera(const Context & context, const Config & config) :
    SingleEndpoint(Span<const DataModel::DeviceTypeEntry>(&Device::Type::kCamera, 1)), mContext(context), mConfig(config)
{}

CHIP_ERROR Camera::Register(chip::EndpointId endpoint, CodeDrivenDataModelProvider & provider, EndpointComposition composition)
{
    VerifyOrReturnError(mEndpointId == kInvalidEndpointId, CHIP_ERROR_INCORRECT_STATE);
    DeviceRegistrationTransaction transaction(*this, provider);

    ReturnErrorOnFailure(RegisterDescriptor(endpoint, provider, composition));

    mCameraAVStreamMgmtCluster.Create(CameraAVStreamManagementCluster::InitArguments{
        .delegate                     = mContext.avsmDelegate,
        .endpointId                   = endpoint,
        .features                     = mConfig.features,
        .optionalAttrs                = mConfig.optionalAttrs,
        .maxConcurrentEncoders        = mConfig.maxConcurrentEncoders,
        .maxEncodedPixelRate          = mConfig.maxEncodedPixelRate,
        .videoSensorParams            = mConfig.videoSensorParams,
        .nightVisionUsesInfrared      = mConfig.nightVisionUsesInfrared,
        .minViewPort                  = mConfig.minViewPort,
        .rateDistortionTradeOffPoints = mConfig.rateDistortionTradeOffPoints,
        .maxContentBufferSize         = mConfig.maxContentBufferSize,
        .microphoneCapabilities       = mConfig.microphoneCapabilities,
        .spkrCapabilities             = mConfig.speakerCapabilities,
        .twoWayTalkSupport            = mConfig.twoWayTalkSupport,
        .snapshotCapabilities         = mConfig.snapshotCapabilities,
        .maxNetworkBandwidth          = mConfig.maxNetworkBandwidth,
        .supportedStreamUsages        = mConfig.supportedStreamUsages,
        .streamUsagePriorities        = mConfig.streamUsagePriorities,
        .defaultAttributeValues       = mConfig.defaultAttributeValues,
    });
    ReturnErrorOnFailure(provider.AddCluster(mCameraAVStreamMgmtCluster.Registration()));

    mWebRTCProviderCluster.Create(endpoint, mContext.webrtcDelegate, mContext.fabricTable);
    ReturnErrorOnFailure(provider.AddCluster(mWebRTCProviderCluster.Registration()));

    ReturnErrorOnFailure(RegisterAdditionalClusters(endpoint, provider));

    ReturnErrorOnFailure(provider.AddEndpoint(mEndpointRegistration));
    transaction.Commit();
    return CHIP_NO_ERROR;
}

void Camera::Unregister(CodeDrivenDataModelProvider & provider)
{
    UnregisterDescriptor(provider);
    UnregisterAdditionalClusters(provider);

    if (mWebRTCProviderCluster.IsConstructed())
    {
        LogErrorOnFailure(provider.RemoveCluster(&mWebRTCProviderCluster.Cluster()));
        mWebRTCProviderCluster.Destroy();
    }
    if (mCameraAVStreamMgmtCluster.IsConstructed())
    {
        LogErrorOnFailure(provider.RemoveCluster(&mCameraAVStreamMgmtCluster.Cluster()));
        mCameraAVStreamMgmtCluster.Destroy();
    }
}

CHIP_ERROR Camera::ClientClusters(ReadOnlyBufferBuilder<ClusterId> & out) const
{
    return out.ReferenceExisting(Span<const ClusterId>(kMandatoryClientClusters));
}

CameraAVStreamManagementCluster & Camera::CameraAVStreamManagementCluster()
{
    VerifyOrDie(mCameraAVStreamMgmtCluster.IsConstructed());
    return mCameraAVStreamMgmtCluster.Cluster();
}

WebRTCTransportProvider::WebRTCTransportProviderCluster & Camera::WebRTCTransportProviderCluster()
{
    VerifyOrDie(mWebRTCProviderCluster.IsConstructed());
    return mWebRTCProviderCluster.Cluster();
}

} // namespace app
} // namespace chip
