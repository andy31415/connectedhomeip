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
#pragma once

#include <app/clusters/camera-av-stream-management-server/CameraAVStreamManagementCluster.h>
#include <app/clusters/webrtc-transport-provider-server/WebRTCTransportProviderCluster.h>
#include <credentials/FabricTable.h>
#include <device/api/SingleEndpoint.h>
#include <lib/support/BitFlags.h>

#include <vector>

namespace chip {
namespace app {

/**
 * @brief Matter Camera device type (0x0142).
 *
 * Registers the spec-mandated server clusters (Camera AV Stream Management and
 * WebRTC Transport Provider) and advertises the mandatory WebRTC Transport
 * Requestor client cluster. Optional clusters may be added by subclasses via
 * RegisterAdditionalClusters() / UnregisterAdditionalClusters().
 */
class Camera : public SingleEndpoint
{
public:
    struct Context
    {
        Clusters::CameraAvStreamManagement::CameraAVStreamManagementDelegate & avsmDelegate;
        Clusters::WebRTCTransportProvider::Delegate & webrtcDelegate;
        FabricTable & fabricTable;
    };

    struct Config
    {
        BitFlags<Clusters::CameraAvStreamManagement::Feature> features{
            Clusters::CameraAvStreamManagement::Feature::kVideo,
            Clusters::CameraAvStreamManagement::Feature::kAudio,
            Clusters::CameraAvStreamManagement::Feature::kSnapshot,
        };
        BitFlags<Clusters::CameraAvStreamManagement::OptionalAttribute> optionalAttrs;
        uint8_t maxConcurrentEncoders                                                 = 1;
        uint32_t maxEncodedPixelRate                                                  = 0;
        Clusters::CameraAvStreamManagement::VideoSensorParamsStruct videoSensorParams = {};
        bool nightVisionUsesInfrared                                                  = false;
        Clusters::CameraAvStreamManagement::VideoResolutionStruct minViewPort         = {};
        std::vector<Clusters::CameraAvStreamManagement::RateDistortionTradeOffStruct> rateDistortionTradeOffPoints;
        uint32_t maxContentBufferSize                                                      = 0;
        Clusters::CameraAvStreamManagement::AudioCapabilitiesStruct microphoneCapabilities = {};
        Clusters::CameraAvStreamManagement::AudioCapabilitiesStruct speakerCapabilities    = {};
        Clusters::CameraAvStreamManagement::TwoWayTalkSupportTypeEnum twoWayTalkSupport =
            Clusters::CameraAvStreamManagement::TwoWayTalkSupportTypeEnum::kNotSupported;
        std::vector<Clusters::CameraAvStreamManagement::SnapshotCapabilitiesStruct> snapshotCapabilities;
        uint32_t maxNetworkBandwidth = 0;
        std::vector<Clusters::Globals::StreamUsageEnum> supportedStreamUsages;
        std::vector<Clusters::Globals::StreamUsageEnum> streamUsagePriorities;
        Clusters::CameraAvStreamManagement::CameraAVStreamManagementCluster::DefaultAttributeValues defaultAttributeValues = {};
    };

    explicit Camera(const Context & context);
    Camera(const Context & context, const Config & config);
    ~Camera() override = default;

    CHIP_ERROR Register(chip::EndpointId endpoint, CodeDrivenDataModelProvider & provider,
                        EndpointComposition composition = {}) override;
    void Unregister(CodeDrivenDataModelProvider & provider) override;

    CHIP_ERROR ClientClusters(ReadOnlyBufferBuilder<ClusterId> & out) const override;

    Clusters::CameraAvStreamManagement::CameraAVStreamManagementCluster & CameraAVStreamManagementCluster();
    Clusters::WebRTCTransportProvider::WebRTCTransportProviderCluster & WebRTCTransportProviderCluster();

protected:
    virtual CHIP_ERROR RegisterAdditionalClusters(chip::EndpointId endpoint, CodeDrivenDataModelProvider & provider)
    {
        return CHIP_NO_ERROR;
    }

    virtual void UnregisterAdditionalClusters(CodeDrivenDataModelProvider & provider) {}

private:
    Context mContext;
    const Config mConfig;

    LazyRegisteredServerCluster<Clusters::CameraAvStreamManagement::CameraAVStreamManagementCluster> mCameraAVStreamMgmtCluster;
    LazyRegisteredServerCluster<Clusters::WebRTCTransportProvider::WebRTCTransportProviderCluster> mWebRTCProviderCluster;
};

} // namespace app
} // namespace chip
