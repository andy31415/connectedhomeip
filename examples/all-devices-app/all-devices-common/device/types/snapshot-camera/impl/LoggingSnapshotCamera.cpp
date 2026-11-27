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
#include <device/types/snapshot-camera/impl/LoggingSnapshotCamera.h>

#include <lib/support/logging/CHIPLogging.h>

#include <algorithm>
#include <cstdint>

using namespace chip::app::Clusters;
using namespace chip::app::Clusters::CameraAvStreamManagement;
using chip::Protocols::InteractionModel::Status;

namespace chip {
namespace app {

namespace {

constexpr uint32_t kDefaultMaxContentBufferSize = 4096;
constexpr uint32_t kDefaultMaxNetworkBandwidth  = 10000000;
constexpr uint16_t kDefaultSnapshotWidth        = 640;
constexpr uint16_t kDefaultSnapshotHeight       = 480;
constexpr uint16_t kDefaultSnapshotMaxFrameRate = 30;

// Minimal valid 1x1 JPEG image returned by CaptureSnapshot().
constexpr uint8_t kDummyJpegImage[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0xFF, 0xDB, 0x00, 0x43, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC0, 0x00, 0x11, 0x08, 0x00, 0x01, 0x00, 0x01, 0x03, 0x01,
    0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xFF, 0xC4, 0x00, 0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
    0x0B, 0xFF, 0xDA, 0x00, 0x0C, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3F, 0x00, 0xF2, 0x8A, 0x28, 0xFF, 0xD9,
};

} // namespace

SnapshotCamera::Config LoggingSnapshotCamera::DefaultConfig()
{
    Config config;
    config.features              = BitFlags<Feature>(Feature::kSnapshot);
    config.maxContentBufferSize  = kDefaultMaxContentBufferSize;
    config.maxNetworkBandwidth   = kDefaultMaxNetworkBandwidth;
    config.snapshotCapabilities  = { { { kDefaultSnapshotWidth, kDefaultSnapshotHeight },
                                       kDefaultSnapshotMaxFrameRate,
                                       ImageCodecEnum::kJpeg,
                                       false,
                                       MakeOptional(false) } };
    config.supportedStreamUsages = { StreamUsageEnum::kLiveView };
    config.streamUsagePriorities = { StreamUsageEnum::kLiveView };
    return config;
}

LoggingSnapshotCamera::LoggingSnapshotCamera(const Config & config) : SnapshotCamera(Context{ *this }, config) {}

Status LoggingSnapshotCamera::VideoStreamAllocate(const VideoStreamStruct & allocateArgs, uint16_t & outStreamID)
{
    return Status::UnsupportedCommand;
}

void LoggingSnapshotCamera::OnVideoStreamAllocated(const VideoStreamStruct & allocatedStream, StreamAllocationAction action) {}

Status LoggingSnapshotCamera::VideoStreamModify(uint16_t streamID, Optional<bool> waterMarkEnabled, Optional<bool> osdEnabled)
{
    return Status::UnsupportedCommand;
}

Status LoggingSnapshotCamera::VideoStreamDeallocate(uint16_t streamID)
{
    return Status::UnsupportedCommand;
}

Status LoggingSnapshotCamera::AudioStreamAllocate(const AudioStreamStruct & allocateArgs, uint16_t & outStreamID)
{
    return Status::UnsupportedCommand;
}

Status LoggingSnapshotCamera::AudioStreamDeallocate(uint16_t streamID)
{
    return Status::UnsupportedCommand;
}

Status LoggingSnapshotCamera::SnapshotStreamAllocate(const SnapshotStreamAllocateArgs & allocateArgs, uint16_t & outStreamID)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, Status::Failure);

    if (auto reusableId = cluster->GetReusableSnapshotStreamId(allocateArgs); reusableId.has_value())
    {
        outStreamID = *reusableId;
        ChipLogProgress(AppServer, "SnapshotCamera: Reusing snapshot stream %u", outStreamID);
        return Status::Success;
    }

    if (allocateArgs.encodedPixels)
    {
        uint32_t candidatePixelRate =
            static_cast<uint32_t>(allocateArgs.maxFrameRate) * allocateArgs.maxResolution.width * allocateArgs.maxResolution.height;
        if (!cluster->IsResourceAvailableForStreamAllocation(candidatePixelRate, allocateArgs.hardwareEncoder))
        {
            return Status::ResourceExhausted;
        }
    }

    outStreamID = mNextSnapshotStreamId++;
    ChipLogProgress(AppServer, "SnapshotCamera: Allocated snapshot stream %u (%ux%u @ %ufps)", outStreamID,
                    allocateArgs.maxResolution.width, allocateArgs.maxResolution.height, allocateArgs.maxFrameRate);
    return Status::Success;
}

Status LoggingSnapshotCamera::SnapshotStreamModify(uint16_t streamID, Optional<bool> waterMarkEnabled, Optional<bool> osdEnabled)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, Status::Failure);

    const auto & streams = cluster->GetAllocatedSnapshotStreams();
    bool found           = std::any_of(streams.begin(), streams.end(),
                                       [streamID](const SnapshotStreamStruct & s) { return s.snapshotStreamID == streamID; });
    VerifyOrReturnError(found, Status::NotFound);

    ChipLogProgress(AppServer, "SnapshotCamera: Modified snapshot stream %u", streamID);
    return Status::Success;
}

Status LoggingSnapshotCamera::SnapshotStreamDeallocate(uint16_t streamID)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, Status::Failure);

    const auto & streams = cluster->GetAllocatedSnapshotStreams();
    bool found           = std::any_of(streams.begin(), streams.end(),
                                       [streamID](const SnapshotStreamStruct & s) { return s.snapshotStreamID == streamID; });
    VerifyOrReturnError(found, Status::NotFound);

    ChipLogProgress(AppServer, "SnapshotCamera: Deallocated snapshot stream %u", streamID);
    return Status::Success;
}

void LoggingSnapshotCamera::OnStreamUsagePrioritiesChanged()
{
    ChipLogProgress(AppServer, "SnapshotCamera: StreamUsagePriorities changed");
}

void LoggingSnapshotCamera::OnAttributeChanged(AttributeId attributeId)
{
    ChipLogProgress(AppServer, "SnapshotCamera: Attribute changed " ChipLogFormatMEI, ChipLogValueMEI(attributeId));
}

Status LoggingSnapshotCamera::CaptureSnapshot(const DataModel::Nullable<uint16_t> streamID,
                                              const VideoResolutionStruct & resolution, ImageSnapshot & outImageSnapshot)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, Status::Failure);

    const auto & streams = cluster->GetAllocatedSnapshotStreams();
    VerifyOrReturnError(!streams.empty(), Status::NotFound);

    const SnapshotStreamStruct * targetStream = &streams.front();
    if (!streamID.IsNull())
    {
        auto it = std::find_if(streams.begin(), streams.end(),
                               [id = streamID.Value()](const SnapshotStreamStruct & s) { return s.snapshotStreamID == id; });
        VerifyOrReturnError(it != streams.end(), Status::NotFound);
        targetStream = &(*it);
    }

    outImageSnapshot.imageCodec = targetStream->imageCodec;
    outImageSnapshot.imageRes   = resolution;
    outImageSnapshot.data.assign(std::begin(kDummyJpegImage), std::end(kDummyJpegImage));

    ChipLogProgress(AppServer, "SnapshotCamera: Captured dummy snapshot (%ux%u, %u bytes)", outImageSnapshot.imageRes.width,
                    outImageSnapshot.imageRes.height, static_cast<unsigned>(outImageSnapshot.data.size()));
    return Status::Success;
}

CHIP_ERROR LoggingSnapshotCamera::PersistentAttributesLoadedCallback()
{
    // Advance mNextSnapshotStreamId past any persisted stream IDs to avoid collisions.
    if (auto * cluster = GetCameraAVStreamManagementCluster(); cluster != nullptr)
    {
        for (const auto & stream : cluster->GetAllocatedSnapshotStreams())
        {
            if (stream.snapshotStreamID >= mNextSnapshotStreamId)
            {
                mNextSnapshotStreamId = static_cast<uint16_t>(stream.snapshotStreamID + 1);
            }
        }
    }
    return CHIP_NO_ERROR;
}

const std::vector<VideoStreamStruct> & LoggingSnapshotCamera::GetAllocatedVideoStreams() const
{
    static const std::vector<VideoStreamStruct> kEmptyVideoStreams;
    return kEmptyVideoStreams;
}

const std::vector<AudioStreamStruct> & LoggingSnapshotCamera::GetAllocatedAudioStreams() const
{
    static const std::vector<AudioStreamStruct> kEmptyAudioStreams;
    return kEmptyAudioStreams;
}

} // namespace app
} // namespace chip
