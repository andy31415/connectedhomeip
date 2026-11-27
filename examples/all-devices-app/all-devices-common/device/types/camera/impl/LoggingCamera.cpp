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
#include <device/types/camera/impl/LoggingCamera.h>

#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

#include <algorithm>
#include <array>
#include <cstdint>

using namespace chip::app::Clusters;
using namespace chip::app::Clusters::CameraAvStreamManagement;
using namespace chip::app::Clusters::WebRTCTransportProvider;
using chip::Protocols::InteractionModel::Status;

namespace chip {
namespace app {

namespace {

constexpr uint16_t kDefaultWidth                = 640;
constexpr uint16_t kDefaultHeight               = 480;
constexpr uint16_t kDefaultFrameRate            = 30;
constexpr uint16_t kDefaultMinViewportWidth     = 320;
constexpr uint16_t kDefaultMinViewportHeight    = 240;
constexpr uint32_t kDefaultMinBitRate           = 500000;
constexpr uint32_t kDefaultMaxContentBufferSize = 4096;
constexpr uint32_t kDefaultMaxNetworkBandwidth  = 10000000;

constexpr uint16_t kSFrameAes128GcmCipherSuite = 1;
constexpr uint16_t kSFrameAes256GcmCipherSuite = 2;
constexpr size_t kSFrameAes128GcmKeyLength     = 16;
constexpr size_t kSFrameAes256GcmKeyLength     = 32;

constexpr std::array<AudioCodecEnum, 1> kSupportedAudioCodecs = { AudioCodecEnum::kOpus };
constexpr std::array<uint32_t, 1> kSupportedSampleRates       = { 48000 };
constexpr std::array<uint8_t, 1> kSupportedBitDepths          = { 16 };

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

Camera::Config LoggingCamera::DefaultConfig()
{
    Config config;
    config.features                     = BitFlags<CameraAvStreamManagement::Feature>(CameraAvStreamManagement::Feature::kVideo,
                                                                                      CameraAvStreamManagement::Feature::kAudio,
                                                                                      CameraAvStreamManagement::Feature::kSnapshot);
    config.maxConcurrentEncoders        = 1;
    config.maxEncodedPixelRate          = static_cast<uint32_t>(kDefaultWidth) * kDefaultHeight * kDefaultFrameRate;
    config.videoSensorParams            = { kDefaultWidth, kDefaultHeight, kDefaultFrameRate, MakeOptional(kDefaultFrameRate) };
    config.minViewPort                  = { kDefaultMinViewportWidth, kDefaultMinViewportHeight };
    config.rateDistortionTradeOffPoints = { { VideoCodecEnum::kH264, { kDefaultWidth, kDefaultHeight }, kDefaultMinBitRate } };
    config.maxContentBufferSize         = kDefaultMaxContentBufferSize;
    config.microphoneCapabilities       = { 1, Span<const AudioCodecEnum>(kSupportedAudioCodecs),
                                            Span<const uint32_t>(kSupportedSampleRates), Span<const uint8_t>(kSupportedBitDepths) };
    config.snapshotCapabilities         = {
        { { kDefaultWidth, kDefaultHeight }, kDefaultFrameRate, ImageCodecEnum::kJpeg, false, MakeOptional(false) }
    };
    config.maxNetworkBandwidth   = kDefaultMaxNetworkBandwidth;
    config.supportedStreamUsages = { Clusters::Globals::StreamUsageEnum::kLiveView,
                                     Clusters::Globals::StreamUsageEnum::kRecording };
    config.streamUsagePriorities = { Clusters::Globals::StreamUsageEnum::kLiveView,
                                     Clusters::Globals::StreamUsageEnum::kRecording };
    return config;
}

LoggingCamera::LoggingCamera(const Context & context, const Config & config) :
    Camera(Camera::Context{ .avsmDelegate = *this, .webrtcDelegate = *this, .fabricTable = context.fabricTable }, config)
{}

Status LoggingCamera::VideoStreamAllocate(const VideoStreamStruct & allocateArgs, uint16_t & outStreamID)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, Status::Failure);

    if (auto reusableId = cluster->GetReusableVideoStreamId(allocateArgs); reusableId.has_value())
    {
        outStreamID = *reusableId;
        return Status::Success;
    }

    uint32_t candidatePixelRate =
        static_cast<uint32_t>(allocateArgs.maxFrameRate) * allocateArgs.maxResolution.width * allocateArgs.maxResolution.height;
    if (!cluster->IsResourceAvailableForStreamAllocation(candidatePixelRate, /* encoderRequired = */ true))
    {
        return Status::ResourceExhausted;
    }

    outStreamID = mNextVideoStreamId++;
    ChipLogProgress(AppServer, "Camera: Allocated video stream %u (%ux%u @ %ufps)", outStreamID, allocateArgs.maxResolution.width,
                    allocateArgs.maxResolution.height, allocateArgs.maxFrameRate);
    return Status::Success;
}

void LoggingCamera::OnVideoStreamAllocated(const VideoStreamStruct & allocatedStream, StreamAllocationAction action)
{
    ChipLogProgress(AppServer, "Camera: Video stream %u ready (action=%u)", allocatedStream.videoStreamID,
                    static_cast<unsigned>(action));
}

Status LoggingCamera::VideoStreamModify(uint16_t streamID, Optional<bool> waterMarkEnabled, Optional<bool> osdEnabled)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, Status::Failure);

    const auto & streams = cluster->GetAllocatedVideoStreams();
    bool found           = std::any_of(streams.begin(), streams.end(),
                                       [streamID](const VideoStreamStruct & s) { return s.videoStreamID == streamID; });
    VerifyOrReturnError(found, Status::NotFound);

    ChipLogProgress(AppServer, "Camera: Modified video stream %u", streamID);
    return Status::Success;
}

Status LoggingCamera::VideoStreamDeallocate(uint16_t streamID)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, Status::Failure);

    const auto & streams = cluster->GetAllocatedVideoStreams();
    bool found           = std::any_of(streams.begin(), streams.end(),
                                       [streamID](const VideoStreamStruct & s) { return s.videoStreamID == streamID; });
    VerifyOrReturnError(found, Status::NotFound);

    ChipLogProgress(AppServer, "Camera: Deallocated video stream %u", streamID);
    return Status::Success;
}

Status LoggingCamera::AudioStreamAllocate(const AudioStreamStruct & allocateArgs, uint16_t & outStreamID)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, Status::Failure);

    const auto & micCaps = cluster->GetMicrophoneCapabilities();
    if (allocateArgs.channelCount > micCaps.maxNumberOfChannels)
    {
        return Status::DynamicConstraintError;
    }

    bool codecSupported = std::any_of(micCaps.supportedCodecs.begin(), micCaps.supportedCodecs.end(),
                                      [&](AudioCodecEnum codec) { return codec == allocateArgs.audioCodec; });
    bool rateSupported  = std::any_of(micCaps.supportedSampleRates.begin(), micCaps.supportedSampleRates.end(),
                                      [&](uint32_t rate) { return rate == allocateArgs.sampleRate; });
    bool depthSupported = std::any_of(micCaps.supportedBitDepths.begin(), micCaps.supportedBitDepths.end(),
                                      [&](uint8_t depth) { return depth == allocateArgs.bitDepth; });
    if (!codecSupported || !rateSupported || !depthSupported)
    {
        return Status::DynamicConstraintError;
    }

    outStreamID = mNextAudioStreamId++;
    ChipLogProgress(AppServer, "Camera: Allocated audio stream %u", outStreamID);
    return Status::Success;
}

Status LoggingCamera::AudioStreamDeallocate(uint16_t streamID)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, Status::Failure);

    const auto & streams = cluster->GetAllocatedAudioStreams();
    bool found           = std::any_of(streams.begin(), streams.end(),
                                       [streamID](const AudioStreamStruct & s) { return s.audioStreamID == streamID; });
    VerifyOrReturnError(found, Status::NotFound);

    ChipLogProgress(AppServer, "Camera: Deallocated audio stream %u", streamID);
    return Status::Success;
}

Status LoggingCamera::SnapshotStreamAllocate(const SnapshotStreamAllocateArgs & allocateArgs, uint16_t & outStreamID)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, Status::Failure);

    if (auto reusableId = cluster->GetReusableSnapshotStreamId(allocateArgs); reusableId.has_value())
    {
        outStreamID = *reusableId;
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
    ChipLogProgress(AppServer, "Camera: Allocated snapshot stream %u (%ux%u @ %ufps)", outStreamID,
                    allocateArgs.maxResolution.width, allocateArgs.maxResolution.height, allocateArgs.maxFrameRate);
    return Status::Success;
}

Status LoggingCamera::SnapshotStreamModify(uint16_t streamID, Optional<bool> waterMarkEnabled, Optional<bool> osdEnabled)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, Status::Failure);

    const auto & streams = cluster->GetAllocatedSnapshotStreams();
    bool found           = std::any_of(streams.begin(), streams.end(),
                                       [streamID](const SnapshotStreamStruct & s) { return s.snapshotStreamID == streamID; });
    VerifyOrReturnError(found, Status::NotFound);

    ChipLogProgress(AppServer, "Camera: Modified snapshot stream %u", streamID);
    return Status::Success;
}

Status LoggingCamera::SnapshotStreamDeallocate(uint16_t streamID)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, Status::Failure);

    const auto & streams = cluster->GetAllocatedSnapshotStreams();
    bool found           = std::any_of(streams.begin(), streams.end(),
                                       [streamID](const SnapshotStreamStruct & s) { return s.snapshotStreamID == streamID; });
    VerifyOrReturnError(found, Status::NotFound);

    ChipLogProgress(AppServer, "Camera: Deallocated snapshot stream %u", streamID);
    return Status::Success;
}

void LoggingCamera::OnStreamUsagePrioritiesChanged()
{
    ChipLogProgress(AppServer, "Camera: StreamUsagePriorities changed");
}

void LoggingCamera::OnAttributeChanged(AttributeId attributeId)
{
    ChipLogProgress(AppServer, "Camera: Attribute changed " ChipLogFormatMEI, ChipLogValueMEI(attributeId));
}

Status LoggingCamera::CaptureSnapshot(const DataModel::Nullable<uint16_t> streamID, const VideoResolutionStruct & resolution,
                                      ImageSnapshot & outImageSnapshot)
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

    ChipLogProgress(AppServer, "Camera: Captured dummy snapshot (%ux%u, %u bytes)", outImageSnapshot.imageRes.width,
                    outImageSnapshot.imageRes.height, static_cast<unsigned>(outImageSnapshot.data.size()));
    return Status::Success;
}

CHIP_ERROR LoggingCamera::PersistentAttributesLoadedCallback()
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, CHIP_NO_ERROR);

    for (const auto & stream : cluster->GetAllocatedVideoStreams())
    {
        if (stream.videoStreamID >= mNextVideoStreamId)
        {
            mNextVideoStreamId = static_cast<uint16_t>(stream.videoStreamID + 1);
        }
    }
    for (const auto & stream : cluster->GetAllocatedAudioStreams())
    {
        if (stream.audioStreamID >= mNextAudioStreamId)
        {
            mNextAudioStreamId = static_cast<uint16_t>(stream.audioStreamID + 1);
        }
    }
    for (const auto & stream : cluster->GetAllocatedSnapshotStreams())
    {
        if (stream.snapshotStreamID >= mNextSnapshotStreamId)
        {
            mNextSnapshotStreamId = static_cast<uint16_t>(stream.snapshotStreamID + 1);
        }
    }
    return CHIP_NO_ERROR;
}

const std::vector<VideoStreamStruct> & LoggingCamera::GetAllocatedVideoStreams() const
{
    if (auto * cluster = GetCameraAVStreamManagementCluster(); cluster != nullptr)
    {
        return cluster->GetAllocatedVideoStreams();
    }
    static const std::vector<VideoStreamStruct> kEmptyVideoStreams;
    return kEmptyVideoStreams;
}

const std::vector<AudioStreamStruct> & LoggingCamera::GetAllocatedAudioStreams() const
{
    if (auto * cluster = GetCameraAVStreamManagementCluster(); cluster != nullptr)
    {
        return cluster->GetAllocatedAudioStreams();
    }
    static const std::vector<AudioStreamStruct> kEmptyAudioStreams;
    return kEmptyAudioStreams;
}

CHIP_ERROR LoggingCamera::HandleSolicitOffer(const OfferRequestArgs & args, WebRTCSessionStruct & outSession,
                                             bool & outDeferredOffer)
{
    ChipLogProgress(AppServer, "Camera: HandleSolicitOffer stub (sessionId=%u)", args.sessionId);
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR LoggingCamera::HandleProvideOffer(const ProvideOfferRequestArgs & args, WebRTCSessionStruct & outSession)
{
    ChipLogProgress(AppServer, "Camera: HandleProvideOffer stub (sessionId=%u)", args.sessionId);
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR LoggingCamera::HandleProvideAnswer(uint16_t sessionId, const std::string & sdpAnswer)
{
    ChipLogProgress(AppServer, "Camera: HandleProvideAnswer stub (sessionId=%u)", sessionId);
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR LoggingCamera::HandleProvideICECandidates(uint16_t sessionId, const std::vector<ICECandidateStruct> & candidates)
{
    ChipLogProgress(AppServer, "Camera: HandleProvideICECandidates stub (sessionId=%u)", sessionId);
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR LoggingCamera::HandleEndSession(uint16_t sessionId, WebRTCEndReasonEnum reasonCode)
{
    ChipLogProgress(AppServer, "Camera: HandleEndSession stub (sessionId=%u)", sessionId);
    return CHIP_ERROR_NOT_FOUND;
}

CHIP_ERROR LoggingCamera::ValidateStreamUsage(Clusters::Globals::StreamUsageEnum streamUsage,
                                              Optional<std::vector<uint16_t>> & videoStreams,
                                              Optional<std::vector<uint16_t>> & audioStreams)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, CHIP_ERROR_INCORRECT_STATE);

    const auto & supportedUsages = cluster->GetSupportedStreamUsages();
    if (std::find(supportedUsages.begin(), supportedUsages.end(), streamUsage) == supportedUsages.end())
    {
        return CHIP_ERROR_NOT_FOUND;
    }

    if (videoStreams.HasValue() && videoStreams.Value().empty())
    {
        const auto & allocated = cluster->GetAllocatedVideoStreams();
        auto exactIt           = std::find_if(allocated.begin(), allocated.end(),
                                              [streamUsage](const VideoStreamStruct & s) { return s.streamUsage == streamUsage; });
        if (exactIt != allocated.end())
        {
            videoStreams.Value().push_back(exactIt->videoStreamID);
        }
        else if (!allocated.empty())
        {
            videoStreams.Value().push_back(allocated.front().videoStreamID);
        }
    }

    if (audioStreams.HasValue() && audioStreams.Value().empty())
    {
        const auto & allocated = cluster->GetAllocatedAudioStreams();
        auto exactIt           = std::find_if(allocated.begin(), allocated.end(),
                                              [streamUsage](const AudioStreamStruct & s) { return s.streamUsage == streamUsage; });
        if (exactIt != allocated.end())
        {
            audioStreams.Value().push_back(exactIt->audioStreamID);
        }
        else if (!allocated.empty())
        {
            audioStreams.Value().push_back(allocated.front().audioStreamID);
        }
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR LoggingCamera::ValidateVideoStreamID(uint16_t videoStreamId)
{
    const auto & streams = GetAllocatedVideoStreams();
    bool found           = std::any_of(streams.begin(), streams.end(),
                                       [videoStreamId](const VideoStreamStruct & s) { return s.videoStreamID == videoStreamId; });
    return found ? CHIP_NO_ERROR : CHIP_ERROR_NOT_FOUND;
}

CHIP_ERROR LoggingCamera::ValidateAudioStreamID(uint16_t audioStreamId)
{
    const auto & streams = GetAllocatedAudioStreams();
    bool found           = std::any_of(streams.begin(), streams.end(),
                                       [audioStreamId](const AudioStreamStruct & s) { return s.audioStreamID == audioStreamId; });
    return found ? CHIP_NO_ERROR : CHIP_ERROR_NOT_FOUND;
}

CHIP_ERROR LoggingCamera::ValidateVideoStreams(const std::vector<uint16_t> & videoStreams)
{
    for (uint16_t id : videoStreams)
    {
        ReturnErrorOnFailure(ValidateVideoStreamID(id));
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR LoggingCamera::ValidateAudioStreams(const std::vector<uint16_t> & audioStreams)
{
    for (uint16_t id : audioStreams)
    {
        ReturnErrorOnFailure(ValidateAudioStreamID(id));
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR LoggingCamera::IsStreamUsageSupported(Clusters::Globals::StreamUsageEnum streamUsage)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, CHIP_ERROR_INCORRECT_STATE);

    const auto & priorities = cluster->GetStreamUsagePriorities();
    bool found              = std::find(priorities.begin(), priorities.end(), streamUsage) != priorities.end();
    return found ? CHIP_NO_ERROR : CHIP_ERROR_NOT_FOUND;
}

CHIP_ERROR LoggingCamera::IsHardPrivacyModeActive(bool & isActive)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, CHIP_ERROR_INCORRECT_STATE);
    isActive = cluster->GetHardPrivacyModeOn();
    return CHIP_NO_ERROR;
}

CHIP_ERROR LoggingCamera::IsSoftRecordingPrivacyModeActive(bool & isActive)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, CHIP_ERROR_INCORRECT_STATE);
    isActive = cluster->GetSoftRecordingPrivacyModeEnabled();
    return CHIP_NO_ERROR;
}

CHIP_ERROR LoggingCamera::IsSoftLivestreamPrivacyModeActive(bool & isActive)
{
    auto * cluster = GetCameraAVStreamManagementCluster();
    VerifyOrReturnError(cluster != nullptr, CHIP_ERROR_INCORRECT_STATE);
    isActive = cluster->GetSoftLivestreamPrivacyModeEnabled();
    return CHIP_NO_ERROR;
}

bool LoggingCamera::HasAllocatedVideoStreams()
{
    return !GetAllocatedVideoStreams().empty();
}

bool LoggingCamera::HasAllocatedAudioStreams()
{
    return !GetAllocatedAudioStreams().empty();
}

CHIP_ERROR LoggingCamera::ValidateSFrameConfig(uint16_t cipherSuite, size_t baseKeyLength)
{
    if (cipherSuite == kSFrameAes128GcmCipherSuite && baseKeyLength == kSFrameAes128GcmKeyLength)
    {
        return CHIP_NO_ERROR;
    }
    if (cipherSuite == kSFrameAes256GcmCipherSuite && baseKeyLength == kSFrameAes256GcmKeyLength)
    {
        return CHIP_NO_ERROR;
    }
    return CHIP_ERROR_INVALID_ARGUMENT;
}

CHIP_ERROR LoggingCamera::IsUTCTimeNull(bool & isNull)
{
    isNull = true;
    return CHIP_NO_ERROR;
}

} // namespace app
} // namespace chip
