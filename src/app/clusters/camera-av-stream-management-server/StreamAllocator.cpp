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

#include <app/MessageDef/StatusIB.h>
#include <app/clusters/camera-av-stream-management-server/StreamAllocator.h>
#include <clusters/CameraAvStreamManagement/Ids.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

#include <algorithm>
#include <limits>

using chip::Protocols::InteractionModel::Status;

namespace chip {
namespace app {
namespace Clusters {
namespace CameraAvStreamManagement {

StreamAllocator::StreamAllocator(MediaControl & mediaControl, Config config) :
    mMediaControl(mediaControl), mPrivacyObserver(config.privacyObserver),
    mSnapshotTemplates(std::move(config.availableSnapshotStreams))
{
    mVideoStreams.reserve(config.availableVideoStreams.size());
    for (const auto & stream : config.availableVideoStreams)
    {
        mVideoStreams.push_back({ stream, false, false });
    }

    mAudioStreams.reserve(config.availableAudioStreams.size());
    for (const auto & stream : config.availableAudioStreams)
    {
        mAudioStreams.push_back({ stream, false });
    }
}

bool StreamAllocator::IsVideoStreamCompatible(const VideoStreamStruct & candidate, const VideoStreamStruct & requested)
{
    return candidate.videoCodec == requested.videoCodec && candidate.minFrameRate <= requested.minFrameRate &&
        candidate.maxFrameRate >= requested.maxFrameRate && candidate.minResolution.width <= requested.minResolution.width &&
        candidate.minResolution.height <= requested.minResolution.height &&
        candidate.maxResolution.width >= requested.maxResolution.width &&
        candidate.maxResolution.height >= requested.maxResolution.height && candidate.minBitRate <= requested.minBitRate &&
        candidate.maxBitRate >= requested.maxBitRate && requested.keyFrameInterval != 0;
}

bool StreamAllocator::IsAudioStreamCompatible(const AudioStreamStruct & candidate, const AudioStreamStruct & requested)
{
    return candidate.audioCodec == requested.audioCodec && candidate.channelCount >= requested.channelCount &&
        candidate.sampleRate >= requested.sampleRate && candidate.bitDepth >= requested.bitDepth;
}

bool StreamAllocator::IsSnapshotStreamCompatible(const SnapshotStreamStruct & candidate,
                                                 const SnapshotStreamAllocateArgs & requested)
{
    return candidate.imageCodec == requested.imageCodec && candidate.frameRate <= requested.maxFrameRate &&
        candidate.minResolution.width <= requested.minResolution.width &&
        candidate.minResolution.height <= requested.minResolution.height &&
        candidate.maxResolution.width >= requested.maxResolution.width &&
        candidate.maxResolution.height >= requested.maxResolution.height;
}

Status StreamAllocator::VideoStreamAllocate(const VideoStreamStruct & allocateArgs, uint16_t & outStreamID)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnValue(cluster != nullptr, Status::Failure);

    const bool supported = std::any_of(mVideoStreams.begin(), mVideoStreams.end(), [&](const VideoEntry & entry) {
        return IsVideoStreamCompatible(entry.params, allocateArgs);
    });
    if (!supported)
    {
        return Status::DynamicConstraintError;
    }

    std::optional<uint16_t> reusableStreamId = cluster->GetReusableVideoStreamId(allocateArgs);
    if (reusableStreamId.has_value())
    {
        outStreamID = reusableStreamId.value();
        return Status::Success;
    }

    for (auto & entry : mVideoStreams)
    {
        if (entry.isAllocated || !IsVideoStreamCompatible(entry.params, allocateArgs))
        {
            continue;
        }

        uint64_t candidateEncodedPixelRate =
            static_cast<uint64_t>(allocateArgs.maxFrameRate) * allocateArgs.maxResolution.height * allocateArgs.maxResolution.width;
        if (candidateEncodedPixelRate > std::numeric_limits<uint32_t>::max() ||
            !cluster->IsResourceAvailableForStreamAllocation(static_cast<uint32_t>(candidateEncodedPixelRate),
                                                             /* encoderRequired = */ true))
        {
            return Status::ResourceExhausted;
        }

        VideoStreamStruct streamToStart = allocateArgs;
        streamToStart.videoStreamID     = entry.params.videoStreamID;
        CHIP_ERROR err                  = mMediaControl.StartVideoStream(streamToStart);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(Camera, "StreamAllocator: StartVideoStream(%u) failed: %" CHIP_ERROR_FORMAT, streamToStart.videoStreamID,
                         err.Format());
            return Status::Failure;
        }

        entry.isAllocated = true;
        entry.isStarted   = true;
        outStreamID       = entry.params.videoStreamID;
        return Status::Success;
    }

    return Status::ResourceExhausted;
}

void StreamAllocator::OnVideoStreamAllocated(const VideoStreamStruct & allocatedStream, StreamAllocationAction action)
{
    auto it = std::find_if(mVideoStreams.begin(), mVideoStreams.end(),
                           [&](const VideoEntry & entry) { return entry.params.videoStreamID == allocatedStream.videoStreamID; });

    switch (action)
    {
    case StreamAllocationAction::kNewAllocation:
        // VideoStreamAllocate already starts newly allocated streams so errors propagate to the caller.
        if (it != mVideoStreams.end() && !it->isStarted)
        {
            CHIP_ERROR err = mMediaControl.StartVideoStream(allocatedStream);
            if (err == CHIP_NO_ERROR)
            {
                it->isStarted = true;
            }
            else
            {
                ChipLogError(Camera, "StreamAllocator: StartVideoStream(%u) failed: %" CHIP_ERROR_FORMAT,
                             allocatedStream.videoStreamID, err.Format());
            }
        }
        break;

    case StreamAllocationAction::kModification:
        if (it != mVideoStreams.end() && it->isAllocated)
        {
            CHIP_ERROR stopErr = mMediaControl.StopVideoStream(allocatedStream.videoStreamID);
            if (stopErr != CHIP_NO_ERROR)
            {
                ChipLogError(Camera, "StreamAllocator: StopVideoStream(%u) during modify failed: %" CHIP_ERROR_FORMAT,
                             allocatedStream.videoStreamID, stopErr.Format());
            }
            CHIP_ERROR startErr = mMediaControl.StartVideoStream(allocatedStream);
            if (startErr != CHIP_NO_ERROR)
            {
                ChipLogError(Camera, "StreamAllocator: StartVideoStream(%u) during modify failed: %" CHIP_ERROR_FORMAT,
                             allocatedStream.videoStreamID, startErr.Format());
                it->isStarted = false;
            }
            else
            {
                it->isStarted = true;
            }
        }
        break;

    case StreamAllocationAction::kReuse:
        break;
    }
}

Status StreamAllocator::VideoStreamModify(uint16_t streamID, Optional<bool> waterMarkEnabled, Optional<bool> osdEnabled)
{
    for (auto & entry : mVideoStreams)
    {
        if (entry.params.videoStreamID == streamID && entry.isAllocated)
        {
            if (waterMarkEnabled.HasValue())
            {
                entry.params.watermarkEnabled = waterMarkEnabled;
            }
            if (osdEnabled.HasValue())
            {
                entry.params.OSDEnabled = osdEnabled;
            }
            return Status::Success;
        }
    }

    return Status::NotFound;
}

Status StreamAllocator::VideoStreamDeallocate(uint16_t streamID)
{
    for (auto & entry : mVideoStreams)
    {
        if (entry.params.videoStreamID == streamID && entry.isAllocated)
        {
            CHIP_ERROR err = mMediaControl.StopVideoStream(streamID);
            if (err != CHIP_NO_ERROR)
            {
                ChipLogError(Camera, "StreamAllocator: StopVideoStream(%u) failed: %" CHIP_ERROR_FORMAT, streamID, err.Format());
                return Status::Failure;
            }
            entry.isAllocated = false;
            entry.isStarted   = false;
            return Status::Success;
        }
    }

    return Status::NotFound;
}

Status StreamAllocator::AudioStreamAllocate(const AudioStreamStruct & allocateArgs, uint16_t & outStreamID)
{
    for (auto & entry : mAudioStreams)
    {
        if (!IsAudioStreamCompatible(entry.params, allocateArgs))
        {
            continue;
        }

        if (!entry.isAllocated)
        {
            AudioStreamStruct streamToStart = allocateArgs;
            streamToStart.audioStreamID     = entry.params.audioStreamID;
            CHIP_ERROR err                  = mMediaControl.StartAudioStream(streamToStart);
            if (err != CHIP_NO_ERROR)
            {
                ChipLogError(Camera, "StreamAllocator: StartAudioStream(%u) failed: %" CHIP_ERROR_FORMAT,
                             streamToStart.audioStreamID, err.Format());
                return Status::Failure;
            }
            entry.isAllocated = true;
        }

        outStreamID = entry.params.audioStreamID;
        return Status::Success;
    }

    return Status::DynamicConstraintError;
}

Status StreamAllocator::AudioStreamDeallocate(uint16_t streamID)
{
    for (auto & entry : mAudioStreams)
    {
        if (entry.params.audioStreamID == streamID && entry.isAllocated)
        {
            CHIP_ERROR err = mMediaControl.StopAudioStream(streamID);
            if (err != CHIP_NO_ERROR)
            {
                ChipLogError(Camera, "StreamAllocator: StopAudioStream(%u) failed: %" CHIP_ERROR_FORMAT, streamID, err.Format());
                return Status::Failure;
            }
            entry.isAllocated = false;
            return Status::Success;
        }
    }

    return Status::NotFound;
}

CHIP_ERROR StreamAllocator::AllocateNextSnapshotStreamId(uint16_t & outStreamId) const
{
    auto isUsed = [&](uint16_t id) {
        return std::any_of(mAllocatedSnapshots.begin(), mAllocatedSnapshots.end(),
                           [id](const SnapshotStreamStruct & s) { return s.snapshotStreamID == id; });
    };

    for (const auto & tmpl : mSnapshotTemplates)
    {
        if (!isUsed(tmpl.snapshotStreamID))
        {
            outStreamId = tmpl.snapshotStreamID;
            return CHIP_NO_ERROR;
        }
    }

    for (uint32_t candidate = 0; candidate <= std::numeric_limits<uint16_t>::max(); ++candidate)
    {
        uint16_t id = static_cast<uint16_t>(candidate);
        if (!isUsed(id))
        {
            outStreamId = id;
            return CHIP_NO_ERROR;
        }
    }

    return CHIP_ERROR_NO_MEMORY;
}

Status StreamAllocator::SnapshotStreamAllocate(const SnapshotStreamAllocateArgs & allocateArgs, uint16_t & outStreamID)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnValue(cluster != nullptr, Status::Failure);

    const bool supported =
        std::any_of(mSnapshotTemplates.begin(), mSnapshotTemplates.end(),
                    [&](const SnapshotStreamStruct & tmpl) { return IsSnapshotStreamCompatible(tmpl, allocateArgs); });
    if (!supported)
    {
        return Status::DynamicConstraintError;
    }

    std::optional<uint16_t> reusableStreamId = cluster->GetReusableSnapshotStreamId(allocateArgs);
    if (reusableStreamId.has_value())
    {
        outStreamID = reusableStreamId.value();
        for (auto & existing : mAllocatedSnapshots)
        {
            if (existing.snapshotStreamID == outStreamID)
            {
                uint16_t origMinWidth  = existing.minResolution.width;
                uint16_t origMinHeight = existing.minResolution.height;

                existing.minResolution.width  = std::max(existing.minResolution.width, allocateArgs.minResolution.width);
                existing.minResolution.height = std::max(existing.minResolution.height, allocateArgs.minResolution.height);
                existing.maxResolution.width  = std::min(existing.maxResolution.width, allocateArgs.maxResolution.width);
                existing.maxResolution.height = std::min(existing.maxResolution.height, allocateArgs.maxResolution.height);

                if (origMinWidth != existing.minResolution.width || origMinHeight != existing.minResolution.height)
                {
                    if (mMediaControl.StopSnapshotStream(outStreamID) != CHIP_NO_ERROR ||
                        mMediaControl.StartSnapshotStream(existing) != CHIP_NO_ERROR)
                    {
                        return Status::Failure;
                    }
                }
                break;
            }
        }
        return Status::Success;
    }

    uint64_t candidateEncodedPixelRate = 0;
    bool encoderRequired               = false;
    if (allocateArgs.encodedPixels)
    {
        candidateEncodedPixelRate =
            static_cast<uint64_t>(allocateArgs.maxFrameRate) * allocateArgs.maxResolution.height * allocateArgs.maxResolution.width;
        encoderRequired = allocateArgs.hardwareEncoder;
    }

    if (candidateEncodedPixelRate > std::numeric_limits<uint32_t>::max() ||
        !cluster->IsResourceAvailableForStreamAllocation(static_cast<uint32_t>(candidateEncodedPixelRate), encoderRequired))
    {
        return Status::ResourceExhausted;
    }

    uint16_t newStreamId = 0;
    if (AllocateNextSnapshotStreamId(newStreamId) != CHIP_NO_ERROR)
    {
        return Status::ResourceExhausted;
    }

    SnapshotStreamStruct newStream;
    newStream.snapshotStreamID = newStreamId;
    newStream.imageCodec       = allocateArgs.imageCodec;
    newStream.frameRate        = allocateArgs.maxFrameRate;
    newStream.minResolution    = allocateArgs.minResolution;
    newStream.maxResolution    = allocateArgs.maxResolution;
    newStream.quality          = allocateArgs.quality;
    newStream.referenceCount   = 0;
    newStream.encodedPixels    = allocateArgs.encodedPixels;
    newStream.hardwareEncoder  = allocateArgs.hardwareEncoder;
    newStream.watermarkEnabled = allocateArgs.watermarkEnabled;
    newStream.OSDEnabled       = allocateArgs.OSDEnabled;

    CHIP_ERROR err = mMediaControl.StartSnapshotStream(newStream);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Camera, "StreamAllocator: StartSnapshotStream(%u) failed: %" CHIP_ERROR_FORMAT, newStreamId, err.Format());
        return Status::Failure;
    }

    mAllocatedSnapshots.push_back(newStream);
    outStreamID = newStreamId;
    return Status::Success;
}

Status StreamAllocator::SnapshotStreamModify(uint16_t streamID, Optional<bool> waterMarkEnabled, Optional<bool> osdEnabled)
{
    for (auto & existing : mAllocatedSnapshots)
    {
        if (existing.snapshotStreamID == streamID)
        {
            if (waterMarkEnabled.HasValue())
            {
                existing.watermarkEnabled = waterMarkEnabled;
            }
            if (osdEnabled.HasValue())
            {
                existing.OSDEnabled = osdEnabled;
            }
            return Status::Success;
        }
    }

    return Status::NotFound;
}

Status StreamAllocator::SnapshotStreamDeallocate(uint16_t streamID)
{
    auto it = std::find_if(mAllocatedSnapshots.begin(), mAllocatedSnapshots.end(),
                           [streamID](const SnapshotStreamStruct & s) { return s.snapshotStreamID == streamID; });
    if (it == mAllocatedSnapshots.end())
    {
        return Status::NotFound;
    }

    CHIP_ERROR err = mMediaControl.StopSnapshotStream(streamID);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Camera, "StreamAllocator: StopSnapshotStream(%u) failed: %" CHIP_ERROR_FORMAT, streamID, err.Format());
        return Status::Failure;
    }

    mAllocatedSnapshots.erase(it);
    return Status::Success;
}

void StreamAllocator::OnStreamUsagePrioritiesChanged()
{
    // CameraAVStreamManagementCluster::SetStreamUsagePriorities also calls OnAttributeChanged(StreamUsagePriorities::Id).
}

void StreamAllocator::OnAttributeChanged(AttributeId attributeId)
{
    mMediaControl.OnAttributeChanged(attributeId);

    auto * cluster = GetCameraAVStreamManagementCluster();
    if (mPrivacyObserver == nullptr || cluster == nullptr)
    {
        return;
    }

    switch (attributeId)
    {
    case Attributes::SoftLivestreamPrivacyModeEnabled::Id:
        mPrivacyObserver->OnSoftLivestreamPrivacyModeChanged(cluster->GetSoftLivestreamPrivacyModeEnabled());
        break;
    case Attributes::SoftRecordingPrivacyModeEnabled::Id:
        mPrivacyObserver->OnSoftRecordingPrivacyModeChanged(cluster->GetSoftRecordingPrivacyModeEnabled());
        break;
    case Attributes::HardPrivacyModeOn::Id:
        mPrivacyObserver->OnHardPrivacyModeChanged(cluster->GetHardPrivacyModeOn());
        break;
    default:
        break;
    }
}

Status StreamAllocator::CaptureSnapshot(DataModel::Nullable<uint16_t> streamID, const VideoResolutionStruct & resolution,
                                        ImageSnapshot & outImageSnapshot)
{
    std::optional<uint16_t> optStreamId = streamID.IsNull() ? std::nullopt : std::make_optional(streamID.Value());
    CHIP_ERROR err                      = mMediaControl.CaptureSnapshot(optStreamId, resolution, outImageSnapshot);
    if (err != CHIP_NO_ERROR)
    {
        return err.IsIMStatus() ? StatusIB(err).mStatus : Status::Failure;
    }
    return Status::Success;
}

CHIP_ERROR StreamAllocator::PersistentAttributesLoadedCallback()
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, CHIP_ERROR_INCORRECT_STATE);

    for (const auto & persistedStream : cluster->GetAllocatedVideoStreams())
    {
        auto it = std::find_if(mVideoStreams.begin(), mVideoStreams.end(), [&](const VideoEntry & entry) {
            return entry.params.videoStreamID == persistedStream.videoStreamID;
        });
        if (it != mVideoStreams.end())
        {
            ReturnErrorOnFailure(mMediaControl.StartVideoStream(persistedStream));
            it->isAllocated = true;
            it->isStarted   = true;
        }
    }

    for (const auto & persistedStream : cluster->GetAllocatedAudioStreams())
    {
        auto it = std::find_if(mAudioStreams.begin(), mAudioStreams.end(), [&](const AudioEntry & entry) {
            return entry.params.audioStreamID == persistedStream.audioStreamID;
        });
        if (it != mAudioStreams.end())
        {
            ReturnErrorOnFailure(mMediaControl.StartAudioStream(persistedStream));
            it->isAllocated = true;
        }
    }

    mAllocatedSnapshots.clear();
    for (const auto & persistedStream : cluster->GetAllocatedSnapshotStreams())
    {
        ReturnErrorOnFailure(mMediaControl.StartSnapshotStream(persistedStream));
        mAllocatedSnapshots.push_back(persistedStream);
    }

    return CHIP_NO_ERROR;
}

const std::vector<VideoStreamStruct> & StreamAllocator::GetAllocatedVideoStreams() const
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    if (cluster == nullptr)
    {
        static const std::vector<VideoStreamStruct> kEmptyVideoStreams;
        return kEmptyVideoStreams;
    }
    return cluster->GetAllocatedVideoStreams();
}

const std::vector<AudioStreamStruct> & StreamAllocator::GetAllocatedAudioStreams() const
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    if (cluster == nullptr)
    {
        static const std::vector<AudioStreamStruct> kEmptyAudioStreams;
        return kEmptyAudioStreams;
    }
    return cluster->GetAllocatedAudioStreams();
}

CHIP_ERROR StreamAllocator::AcquireStreams(const std::vector<uint16_t> & audioStreams, const std::vector<uint16_t> & videoStreams)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, CHIP_ERROR_INCORRECT_STATE);

    std::vector<uint16_t> acquiredAudio;
    std::vector<uint16_t> acquiredVideo;

    auto rollback = [&]() {
        for (uint16_t id : acquiredAudio)
        {
            LogErrorOnFailure(cluster->UpdateAudioStreamRefCount(id, /* shouldIncrement = */ false));
        }
        for (uint16_t id : acquiredVideo)
        {
            LogErrorOnFailure(cluster->UpdateVideoStreamRefCount(id, /* shouldIncrement = */ false));
        }
    };

    for (uint16_t id : audioStreams)
    {
        CHIP_ERROR err = cluster->UpdateAudioStreamRefCount(id, /* shouldIncrement = */ true);
        if (err != CHIP_NO_ERROR)
        {
            rollback();
            return err;
        }
        acquiredAudio.push_back(id);
    }

    for (uint16_t id : videoStreams)
    {
        CHIP_ERROR err = cluster->UpdateVideoStreamRefCount(id, /* shouldIncrement = */ true);
        if (err != CHIP_NO_ERROR)
        {
            rollback();
            return err;
        }
        acquiredVideo.push_back(id);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR StreamAllocator::ReleaseStreams(const std::vector<uint16_t> & audioStreams, const std::vector<uint16_t> & videoStreams)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, CHIP_ERROR_INCORRECT_STATE);

    CHIP_ERROR firstErr = CHIP_NO_ERROR;

    for (uint16_t id : audioStreams)
    {
        CHIP_ERROR err = cluster->UpdateAudioStreamRefCount(id, /* shouldIncrement = */ false);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(Camera, "StreamAllocator: failed to decrement audio stream %u refcount: %" CHIP_ERROR_FORMAT, id,
                         err.Format());
            if (firstErr == CHIP_NO_ERROR)
            {
                firstErr = err;
            }
        }
    }

    for (uint16_t id : videoStreams)
    {
        CHIP_ERROR err = cluster->UpdateVideoStreamRefCount(id, /* shouldIncrement = */ false);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(Camera, "StreamAllocator: failed to decrement video stream %u refcount: %" CHIP_ERROR_FORMAT, id,
                         err.Format());
            if (firstErr == CHIP_NO_ERROR)
            {
                firstErr = err;
            }
        }
    }

    return firstErr;
}

} // namespace CameraAvStreamManagement
} // namespace Clusters
} // namespace app
} // namespace chip
