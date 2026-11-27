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
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>

#include <cstdint>
#include <optional>

namespace chip {
namespace app {
namespace Clusters {
namespace CameraAvStreamManagement {

/**
 * Platform media control seam invoked by StreamAllocator on the Matter thread.
 *
 * The SDK owns stream capability matching, allocation, ref-counting, and cluster attributes;
 * the platform starts/stops allocated streams, captures snapshots, and optionally reacts to
 * AVSM attribute changes.
 */
class MediaControl
{
public:
    virtual ~MediaControl() = default;

    virtual CHIP_ERROR StartVideoStream(const VideoStreamStruct & stream)       = 0;
    virtual CHIP_ERROR StopVideoStream(uint16_t streamId)                       = 0;
    virtual CHIP_ERROR StartAudioStream(const AudioStreamStruct & stream)       = 0;
    virtual CHIP_ERROR StopAudioStream(uint16_t streamId)                       = 0;
    virtual CHIP_ERROR StartSnapshotStream(const SnapshotStreamStruct & stream) = 0;
    virtual CHIP_ERROR StopSnapshotStream(uint16_t streamId)                    = 0;
    virtual CHIP_ERROR CaptureSnapshot(std::optional<uint16_t> streamId, const VideoResolutionStruct & resolution,
                                       ImageSnapshot & outSnapshot)             = 0;
    virtual void OnAttributeChanged(AttributeId attributeId) {}
};

} // namespace CameraAvStreamManagement
} // namespace Clusters
} // namespace app
} // namespace chip
