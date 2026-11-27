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
#include <app/clusters/camera-av-stream-management-server/MediaControl.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/Optional.h>
#include <protocols/interaction_model/StatusCode.h>

#include <cstdint>
#include <vector>

namespace chip {
namespace app {
namespace Clusters {
namespace CameraAvStreamManagement {

/**
 * SDK implementation of CameraAVStreamManagementDelegate that owns stream capability
 * matching, reusable-stream lookup, resource checks, stream ref-counting, and
 * persistent-stream restoration, delegating media start/stop/capture to MediaControl.
 */
class StreamAllocator : public CameraAVStreamManagementDelegate
{
public:
    class PrivacyObserver
    {
    public:
        virtual ~PrivacyObserver() = default;
        virtual void OnSoftLivestreamPrivacyModeChanged(bool enabled) {}
        virtual void OnSoftRecordingPrivacyModeChanged(bool enabled) {}
        virtual void OnHardPrivacyModeChanged(bool enabled) {}
    };

    struct Config
    {
        std::vector<VideoStreamStruct> availableVideoStreams;
        std::vector<AudioStreamStruct> availableAudioStreams;
        std::vector<SnapshotStreamStruct> availableSnapshotStreams;
        PrivacyObserver * privacyObserver = nullptr;
    };

    StreamAllocator(MediaControl & mediaControl, Config config);
    ~StreamAllocator() override = default;

    StreamAllocator(const StreamAllocator &)             = delete;
    StreamAllocator & operator=(const StreamAllocator &) = delete;

    void SetPrivacyObserver(PrivacyObserver * observer) { mPrivacyObserver = observer; }

    // CameraAVStreamManagementDelegate
    Protocols::InteractionModel::Status VideoStreamAllocate(const VideoStreamStruct & allocateArgs,
                                                            uint16_t & outStreamID) override;
    void OnVideoStreamAllocated(const VideoStreamStruct & allocatedStream, StreamAllocationAction action) override;
    Protocols::InteractionModel::Status VideoStreamModify(uint16_t streamID, Optional<bool> waterMarkEnabled,
                                                          Optional<bool> osdEnabled) override;
    Protocols::InteractionModel::Status VideoStreamDeallocate(uint16_t streamID) override;

    Protocols::InteractionModel::Status AudioStreamAllocate(const AudioStreamStruct & allocateArgs,
                                                            uint16_t & outStreamID) override;
    Protocols::InteractionModel::Status AudioStreamDeallocate(uint16_t streamID) override;

    Protocols::InteractionModel::Status SnapshotStreamAllocate(const SnapshotStreamAllocateArgs & allocateArgs,
                                                               uint16_t & outStreamID) override;
    Protocols::InteractionModel::Status SnapshotStreamModify(uint16_t streamID, Optional<bool> waterMarkEnabled,
                                                             Optional<bool> osdEnabled) override;
    Protocols::InteractionModel::Status SnapshotStreamDeallocate(uint16_t streamID) override;

    void OnStreamUsagePrioritiesChanged() override;
    void OnAttributeChanged(AttributeId attributeId) override;

    Protocols::InteractionModel::Status CaptureSnapshot(DataModel::Nullable<uint16_t> streamID,
                                                        const VideoResolutionStruct & resolution,
                                                        ImageSnapshot & outImageSnapshot) override;

    CHIP_ERROR PersistentAttributesLoadedCallback() override;

    const std::vector<VideoStreamStruct> & GetAllocatedVideoStreams() const override;
    const std::vector<AudioStreamStruct> & GetAllocatedAudioStreams() const override;

    // Transport ref-counting helpers
    CHIP_ERROR AcquireStreams(const std::vector<uint16_t> & audioStreams, const std::vector<uint16_t> & videoStreams);
    CHIP_ERROR ReleaseStreams(const std::vector<uint16_t> & audioStreams, const std::vector<uint16_t> & videoStreams);

private:
    struct VideoEntry
    {
        VideoStreamStruct params;
        bool isAllocated = false;
        bool isStarted   = false;
    };

    struct AudioEntry
    {
        AudioStreamStruct params;
        bool isAllocated = false;
    };

    static bool IsVideoStreamCompatible(const VideoStreamStruct & candidate, const VideoStreamStruct & requested);
    static bool IsAudioStreamCompatible(const AudioStreamStruct & candidate, const AudioStreamStruct & requested);
    static bool IsSnapshotStreamCompatible(const SnapshotStreamStruct & candidate, const SnapshotStreamAllocateArgs & requested);

    CHIP_ERROR AllocateNextSnapshotStreamId(uint16_t & outStreamId) const;

    MediaControl & mMediaControl;
    PrivacyObserver * mPrivacyObserver = nullptr;
    std::vector<VideoEntry> mVideoStreams;
    std::vector<AudioEntry> mAudioStreams;
    std::vector<SnapshotStreamStruct> mSnapshotTemplates;
    std::vector<SnapshotStreamStruct> mAllocatedSnapshots;
};

} // namespace CameraAvStreamManagement
} // namespace Clusters
} // namespace app
} // namespace chip
