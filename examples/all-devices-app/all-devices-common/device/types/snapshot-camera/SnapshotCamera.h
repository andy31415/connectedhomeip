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
#include <device/api/SingleEndpoint.h>
#include <lib/support/BitFlags.h>

#include <vector>

namespace chip {
namespace app {

/**
 * @brief Matter Snapshot Camera device type (0x0145).
 *
 * Registers the mandatory Camera AV Stream Management server cluster configured
 * for snapshot-only operation (Feature::kSnapshot enabled; Video and Audio
 * disallowed per specification). Optional clusters may be added by subclasses
 * via RegisterAdditionalClusters() / UnregisterAdditionalClusters().
 */
class SnapshotCamera : public SingleEndpoint
{
public:
    struct Context
    {
        Clusters::CameraAvStreamManagement::CameraAVStreamManagementDelegate & avsmDelegate;
    };

    struct Config
    {
        BitFlags<Clusters::CameraAvStreamManagement::Feature> features{ Clusters::CameraAvStreamManagement::Feature::kSnapshot };
        BitFlags<Clusters::CameraAvStreamManagement::OptionalAttribute> optionalAttrs;
        bool nightVisionUsesInfrared  = false;
        uint32_t maxContentBufferSize = 0;
        std::vector<Clusters::CameraAvStreamManagement::SnapshotCapabilitiesStruct> snapshotCapabilities;
        uint32_t maxNetworkBandwidth = 0;
        std::vector<Clusters::Globals::StreamUsageEnum> supportedStreamUsages;
        std::vector<Clusters::Globals::StreamUsageEnum> streamUsagePriorities;
        Clusters::CameraAvStreamManagement::CameraAVStreamManagementCluster::DefaultAttributeValues defaultAttributeValues = {};
    };

    explicit SnapshotCamera(const Context & context);
    SnapshotCamera(const Context & context, const Config & config);
    ~SnapshotCamera() override = default;

    CHIP_ERROR Register(chip::EndpointId endpoint, CodeDrivenDataModelProvider & provider,
                        EndpointComposition composition = {}) override;
    void Unregister(CodeDrivenDataModelProvider & provider) override;

    Clusters::CameraAvStreamManagement::CameraAVStreamManagementCluster & CameraAVStreamManagementCluster();

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
};

} // namespace app
} // namespace chip
