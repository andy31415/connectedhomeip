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

#include "Esp32Camera.h"

#include <lib/support/CodeUtils.h>
#include <system/SystemClock.h>

namespace chip::app {

using namespace chip::app::Clusters;
using namespace chip::app::Clusters::CameraAvStreamManagement;
using namespace chip::app::Clusters::WebRTCTransportProvider;

namespace {

constexpr uint16_t kVideoStreamId        = 1;
constexpr uint16_t kSnapshotStreamId     = 1;
constexpr uint32_t kMaxContentBufferSize = 4096;
constexpr uint32_t kMaxNetworkBandwidth  = 2000000;

} // namespace

Camera::Config Esp32Camera::DefaultConfig()
{
    const uint16_t width   = Esp32MediaControl::kVideoWidth;
    const uint16_t height  = Esp32MediaControl::kVideoHeight;
    const uint16_t fps     = Esp32MediaControl::kTargetFps;
    const uint32_t bitrate = Esp32MediaControl::kTargetBitrate;
    const uint32_t minRate = bitrate >= 100000 ? 100000 : bitrate;
    const uint32_t maxRate = static_cast<uint32_t>(width) * height * fps;

    Config config;
    config.features                     = BitFlags<CameraAvStreamManagement::Feature>(CameraAvStreamManagement::Feature::kVideo,
                                                                                      CameraAvStreamManagement::Feature::kSnapshot,
                                                                                      CameraAvStreamManagement::Feature::kPrivacy);
    config.maxConcurrentEncoders        = 1;
    config.maxEncodedPixelRate          = maxRate;
    config.videoSensorParams            = { width, height, Esp32MediaControl::kSensorFps, MakeOptional(fps) };
    config.minViewPort                  = { width, height };
    config.rateDistortionTradeOffPoints = { { VideoCodecEnum::kH264, { width, height }, minRate } };
    config.maxContentBufferSize         = kMaxContentBufferSize;
    config.snapshotCapabilities         = {
        { { width, height }, Esp32MediaControl::kSensorFps, ImageCodecEnum::kJpeg, false, MakeOptional(false) }
    };
    config.maxNetworkBandwidth   = kMaxNetworkBandwidth;
    config.supportedStreamUsages = { Globals::StreamUsageEnum::kLiveView };
    config.streamUsagePriorities = { Globals::StreamUsageEnum::kLiveView };
    return config;
}

StreamAllocator::Config Esp32Camera::DefaultAllocatorConfig()
{
    const uint16_t width   = Esp32MediaControl::kVideoWidth;
    const uint16_t height  = Esp32MediaControl::kVideoHeight;
    const uint16_t fps     = Esp32MediaControl::kTargetFps;
    const uint32_t bitrate = Esp32MediaControl::kTargetBitrate;

    StreamAllocator::Config config;
    VideoStreamStruct videoTemplate;
    videoTemplate.videoStreamID    = kVideoStreamId;
    videoTemplate.streamUsage      = Globals::StreamUsageEnum::kLiveView;
    videoTemplate.videoCodec       = VideoCodecEnum::kH264;
    videoTemplate.minFrameRate     = 1;
    videoTemplate.maxFrameRate     = Esp32MediaControl::kSensorFps;
    videoTemplate.minResolution    = { width, height };
    videoTemplate.maxResolution    = { width, height };
    videoTemplate.minBitRate       = 50000;
    videoTemplate.maxBitRate       = std::max<uint32_t>(bitrate, 1000000);
    videoTemplate.keyFrameInterval = 10;
    config.availableVideoStreams.push_back(videoTemplate);

    SnapshotStreamStruct snapshotTemplate;
    snapshotTemplate.snapshotStreamID = kSnapshotStreamId;
    snapshotTemplate.imageCodec       = ImageCodecEnum::kJpeg;
    snapshotTemplate.frameRate        = fps;
    snapshotTemplate.minResolution    = { width, height };
    snapshotTemplate.maxResolution    = { width, height };
    snapshotTemplate.quality          = CONFIG_ALL_DEVICES_CAMERA_JPEG_QUALITY;
    snapshotTemplate.encodedPixels    = true;
    snapshotTemplate.hardwareEncoder  = false;
    config.availableSnapshotStreams.push_back(snapshotTemplate);

    return config;
}

Esp32Camera::Esp32Camera(const Context & context) :
    Camera(Camera::Context{ .avsmDelegate = mStreamAllocator, .webrtcDelegate = *this, .fabricTable = context.fabricTable },
           DefaultConfig()),
    mTimerDelegate(context.timerDelegate), mStreamAllocator(mMediaControl, DefaultAllocatorConfig()), mPeerFactory(mMediaControl),
    mRequestorClient(context.caseSessionManager)
{}

Esp32Camera::~Esp32Camera()
{
    mStreamAllocator.SetPrivacyObserver(nullptr);
    mMediaControl.SetAvsmCluster(nullptr);
    if (mSessionDelegate != nullptr)
    {
        mSessionDelegate->Shutdown();
        mSessionDelegate.reset();
    }
}

CHIP_ERROR Esp32Camera::RegisterAdditionalClusters(EndpointId endpoint, CodeDrivenDataModelProvider & provider)
{
    mMediaControl.SetAvsmCluster(&CameraAVStreamManagementCluster());

    WebRTCProviderSessionDelegate::Context delegateContext{
        .peerFactory     = mPeerFactory,
        .requestorClient = mRequestorClient,
        .avsmCluster     = CameraAVStreamManagementCluster(),
        .timerDelegate   = mTimerDelegate,
        .isUtcTimeNull =
            []() {
                System::Clock::Microseconds64 utcTime;
                return System::SystemClock().GetClock_RealTime(utcTime) != CHIP_NO_ERROR;
            },
        .maxConcurrentSessions = 2,
    };

    mSessionDelegate = std::make_unique<WebRTCProviderSessionDelegate>(std::move(delegateContext));
    mSessionDelegate->SetWebRTCTransportProvider(&WebRTCTransportProviderCluster());
    mStreamAllocator.SetPrivacyObserver(mSessionDelegate.get());
    return CHIP_NO_ERROR;
}

void Esp32Camera::UnregisterAdditionalClusters(CodeDrivenDataModelProvider & provider)
{
    mStreamAllocator.SetPrivacyObserver(nullptr);
    mMediaControl.SetAvsmCluster(nullptr);
    if (mSessionDelegate != nullptr)
    {
        mSessionDelegate->SetWebRTCTransportProvider(nullptr);
        mSessionDelegate->Shutdown();
        mSessionDelegate.reset();
    }
}

CHIP_ERROR Esp32Camera::HandleSolicitOffer(const OfferRequestArgs & args, WebRTCSessionStruct & outSession, bool & outDeferredOffer)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->HandleSolicitOffer(args, outSession, outDeferredOffer);
}

CHIP_ERROR Esp32Camera::HandleProvideOffer(const ProvideOfferRequestArgs & args, WebRTCSessionStruct & outSession)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->HandleProvideOffer(args, outSession);
}

CHIP_ERROR Esp32Camera::HandleProvideAnswer(uint16_t sessionId, const std::string & sdp)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->HandleProvideAnswer(sessionId, sdp);
}

CHIP_ERROR Esp32Camera::HandleProvideICECandidates(uint16_t sessionId, const std::vector<ICECandidateStruct> & candidates)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->HandleProvideICECandidates(sessionId, candidates);
}

CHIP_ERROR Esp32Camera::HandleEndSession(uint16_t sessionId, WebRTCEndReasonEnum reasonCode)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->HandleEndSession(sessionId, reasonCode);
}

CHIP_ERROR Esp32Camera::ValidateStreamUsage(StreamUsageEnum streamUsage, Optional<std::vector<uint16_t>> & videoStreams,
                                            Optional<std::vector<uint16_t>> & audioStreams)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->ValidateStreamUsage(streamUsage, videoStreams, audioStreams);
}

CHIP_ERROR Esp32Camera::ValidateVideoStreamID(uint16_t videoStreamId)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->ValidateVideoStreamID(videoStreamId);
}

CHIP_ERROR Esp32Camera::ValidateAudioStreamID(uint16_t audioStreamId)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->ValidateAudioStreamID(audioStreamId);
}

CHIP_ERROR Esp32Camera::ValidateVideoStreams(const std::vector<uint16_t> & videoStreams)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->ValidateVideoStreams(videoStreams);
}

CHIP_ERROR Esp32Camera::ValidateAudioStreams(const std::vector<uint16_t> & audioStreams)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->ValidateAudioStreams(audioStreams);
}

CHIP_ERROR Esp32Camera::IsStreamUsageSupported(StreamUsageEnum streamUsage)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->IsStreamUsageSupported(streamUsage);
}

CHIP_ERROR Esp32Camera::IsHardPrivacyModeActive(bool & isActive)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->IsHardPrivacyModeActive(isActive);
}

CHIP_ERROR Esp32Camera::IsSoftRecordingPrivacyModeActive(bool & isActive)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->IsSoftRecordingPrivacyModeActive(isActive);
}

CHIP_ERROR Esp32Camera::IsSoftLivestreamPrivacyModeActive(bool & isActive)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->IsSoftLivestreamPrivacyModeActive(isActive);
}

bool Esp32Camera::HasAllocatedVideoStreams()
{
    return mSessionDelegate != nullptr && mSessionDelegate->HasAllocatedVideoStreams();
}

bool Esp32Camera::HasAllocatedAudioStreams()
{
    return mSessionDelegate != nullptr && mSessionDelegate->HasAllocatedAudioStreams();
}

CHIP_ERROR Esp32Camera::ValidateSFrameConfig(uint16_t cipherSuite, size_t kidSize)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->ValidateSFrameConfig(cipherSuite, kidSize);
}

CHIP_ERROR Esp32Camera::IsUTCTimeNull(bool & isNull)
{
    VerifyOrReturnError(mSessionDelegate != nullptr, CHIP_ERROR_INCORRECT_STATE);
    return mSessionDelegate->IsUTCTimeNull(isNull);
}

} // namespace chip::app
