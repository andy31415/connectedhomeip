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

#include <device/types/snapshot-camera/SnapshotCamera.h>

#include <cstdint>
#include <vector>

namespace chip {
namespace app {

/**
 * @brief Logging implementation of SnapshotCamera with in-memory snapshot stream
 *        allocation and dummy JPEG snapshot capture.
 */
class LoggingSnapshotCamera : private Clusters::CameraAvStreamManagement::CameraAVStreamManagementDelegate, public SnapshotCamera
{
public:
    static Config DefaultConfig();

    explicit LoggingSnapshotCamera(const Config & config = DefaultConfig());
    ~LoggingSnapshotCamera() override = default;

private:
    // CameraAVStreamManagementDelegate
    Protocols::InteractionModel::Status
    VideoStreamAllocate(const Clusters::CameraAvStreamManagement::VideoStreamStruct & allocateArgs,
                        uint16_t & outStreamID) override;
    void OnVideoStreamAllocated(const Clusters::CameraAvStreamManagement::VideoStreamStruct & allocatedStream,
                                Clusters::CameraAvStreamManagement::StreamAllocationAction action) override;
    Protocols::InteractionModel::Status VideoStreamModify(uint16_t streamID, Optional<bool> waterMarkEnabled,
                                                          Optional<bool> osdEnabled) override;
    Protocols::InteractionModel::Status VideoStreamDeallocate(uint16_t streamID) override;
    Protocols::InteractionModel::Status
    AudioStreamAllocate(const Clusters::CameraAvStreamManagement::AudioStreamStruct & allocateArgs,
                        uint16_t & outStreamID) override;
    Protocols::InteractionModel::Status AudioStreamDeallocate(uint16_t streamID) override;
    Protocols::InteractionModel::Status SnapshotStreamAllocate(const SnapshotStreamAllocateArgs & allocateArgs,
                                                               uint16_t & outStreamID) override;
    Protocols::InteractionModel::Status SnapshotStreamModify(uint16_t streamID, Optional<bool> waterMarkEnabled,
                                                             Optional<bool> osdEnabled) override;
    Protocols::InteractionModel::Status SnapshotStreamDeallocate(uint16_t streamID) override;
    void OnStreamUsagePrioritiesChanged() override;
    void OnAttributeChanged(AttributeId attributeId) override;
    Protocols::InteractionModel::Status
    CaptureSnapshot(const DataModel::Nullable<uint16_t> streamID,
                    const Clusters::CameraAvStreamManagement::VideoResolutionStruct & resolution,
                    Clusters::CameraAvStreamManagement::ImageSnapshot & outImageSnapshot) override;
    CHIP_ERROR PersistentAttributesLoadedCallback() override;
    const std::vector<Clusters::CameraAvStreamManagement::VideoStreamStruct> & GetAllocatedVideoStreams() const override;
    const std::vector<Clusters::CameraAvStreamManagement::AudioStreamStruct> & GetAllocatedAudioStreams() const override;

    uint16_t mNextSnapshotStreamId = 1;
};

} // namespace app
} // namespace chip
