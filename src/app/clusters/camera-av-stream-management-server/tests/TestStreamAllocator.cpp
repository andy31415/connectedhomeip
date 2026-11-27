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

#include <lib/support/tests/ExtraPwTestMacros.h>
#include <pw_unit_test/framework.h>

#include <app/clusters/camera-av-stream-management-server/CameraAVStreamManagementCluster.h>
#include <app/clusters/camera-av-stream-management-server/MediaControl.h>
#include <app/clusters/camera-av-stream-management-server/StreamAllocator.h>
#include <app/server-cluster/testing/ClusterTester.h>
#include <clusters/CameraAvStreamManagement/Attributes.h>
#include <clusters/CameraAvStreamManagement/Commands.h>
#include <lib/core/CHIPError.h>

#include <array>
#include <optional>
#include <vector>

namespace {

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::CameraAvStreamManagement;
using namespace chip::Testing;
using namespace chip::Protocols::InteractionModel;

constexpr EndpointId kTestEndpointId = 1;

class FakeMediaControl : public MediaControl
{
public:
    CHIP_ERROR StartVideoStream(const VideoStreamStruct & stream) override
    {
        ReturnErrorOnFailure(startVideoResult);
        startedVideoStreams.push_back(stream);
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR StopVideoStream(uint16_t streamId) override
    {
        ReturnErrorOnFailure(stopVideoResult);
        stoppedVideoStreamIds.push_back(streamId);
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR StartAudioStream(const AudioStreamStruct & stream) override
    {
        ReturnErrorOnFailure(startAudioResult);
        startedAudioStreams.push_back(stream);
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR StopAudioStream(uint16_t streamId) override
    {
        ReturnErrorOnFailure(stopAudioResult);
        stoppedAudioStreamIds.push_back(streamId);
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR StartSnapshotStream(const SnapshotStreamStruct & stream) override
    {
        ReturnErrorOnFailure(startSnapshotResult);
        startedSnapshotStreams.push_back(stream);
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR StopSnapshotStream(uint16_t streamId) override
    {
        ReturnErrorOnFailure(stopSnapshotResult);
        stoppedSnapshotStreamIds.push_back(streamId);
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR CaptureSnapshot(std::optional<uint16_t> streamId, const VideoResolutionStruct & resolution,
                               ImageSnapshot & outSnapshot) override
    {
        capturedStreamIds.push_back(streamId);
        ReturnErrorOnFailure(captureSnapshotResult);
        outSnapshot.imageCodec = ImageCodecEnum::kJpeg;
        outSnapshot.imageRes   = resolution;
        outSnapshot.data       = { 0xFF, 0xD8, 0xFF, 0xD9 };
        return CHIP_NO_ERROR;
    }

    void OnAttributeChanged(AttributeId attributeId) override { changedAttributes.push_back(attributeId); }

    CHIP_ERROR startVideoResult      = CHIP_NO_ERROR;
    CHIP_ERROR stopVideoResult       = CHIP_NO_ERROR;
    CHIP_ERROR startAudioResult      = CHIP_NO_ERROR;
    CHIP_ERROR stopAudioResult       = CHIP_NO_ERROR;
    CHIP_ERROR startSnapshotResult   = CHIP_NO_ERROR;
    CHIP_ERROR stopSnapshotResult    = CHIP_NO_ERROR;
    CHIP_ERROR captureSnapshotResult = CHIP_NO_ERROR;

    std::vector<VideoStreamStruct> startedVideoStreams;
    std::vector<uint16_t> stoppedVideoStreamIds;
    std::vector<AudioStreamStruct> startedAudioStreams;
    std::vector<uint16_t> stoppedAudioStreamIds;
    std::vector<SnapshotStreamStruct> startedSnapshotStreams;
    std::vector<uint16_t> stoppedSnapshotStreamIds;
    std::vector<std::optional<uint16_t>> capturedStreamIds;
    std::vector<AttributeId> changedAttributes;
};

class RecordingPrivacyObserver : public StreamAllocator::PrivacyObserver
{
public:
    void OnSoftLivestreamPrivacyModeChanged(bool enabled) override
    {
        softLivestreamCount++;
        lastSoftLivestream = enabled;
    }
    void OnSoftRecordingPrivacyModeChanged(bool enabled) override
    {
        softRecordingCount++;
        lastSoftRecording = enabled;
    }
    void OnHardPrivacyModeChanged(bool enabled) override
    {
        hardPrivacyCount++;
        lastHardPrivacy = enabled;
    }

    int softLivestreamCount = 0;
    bool lastSoftLivestream = false;
    int softRecordingCount  = 0;
    bool lastSoftRecording  = false;
    int hardPrivacyCount    = 0;
    bool lastHardPrivacy    = false;
};

StreamAllocator::Config MakeAllocatorConfig(StreamAllocator::PrivacyObserver * observer = nullptr)
{
    StreamAllocator::Config config;
    config.availableVideoStreams = {
        { 1,
          StreamUsageEnum::kLiveView,
          VideoCodecEnum::kH264,
          15,
          60,
          { 320, 240 },
          { 1280, 720 },
          100000,
          4000000,
          1000,
          MakeOptional(false),
          MakeOptional(false),
          0 },
    };
    config.availableAudioStreams = {
        { 1, StreamUsageEnum::kLiveView, AudioCodecEnum::kOpus, 2, 48000, 64000, 24, 0 },
    };
    config.availableSnapshotStreams = {
        { 1,
          ImageCodecEnum::kJpeg,
          15,
          { 320, 240 },
          { 1280, 720 },
          90,
          0,
          false,
          false,
          MakeOptional(false),
          MakeOptional(false) },
    };
    config.privacyObserver = observer;
    return config;
}

CameraAVStreamManagementCluster::InitArguments MakeClusterArgs(CameraAVStreamManagementDelegate & delegate)
{
    static std::array<AudioCodecEnum, 1> audioCodecs = { AudioCodecEnum::kOpus };
    static std::array<uint32_t, 1> sampleRates       = { 48000 };
    static std::array<uint8_t, 1> bitDepths          = { 24 };
    static AudioCapabilitiesStruct audioCaps         = { 2, Span<AudioCodecEnum>(audioCodecs), Span<uint32_t>(sampleRates),
                                                         Span<uint8_t>(bitDepths) };

    return CameraAVStreamManagementCluster::InitArguments{
        .delegate                     = delegate,
        .endpointId                   = kTestEndpointId,
        .features                     = BitFlags<Feature>(Feature::kVideo, Feature::kAudio, Feature::kSnapshot, Feature::kPrivacy),
        .optionalAttrs                = BitFlags<OptionalAttribute>(OptionalAttribute::kHardPrivacyModeOn),
        .maxConcurrentEncoders        = 2,
        .maxEncodedPixelRate          = 1280 * 720 * 60,
        .videoSensorParams            = { 1280, 720, 60, Optional<uint16_t>() },
        .minViewPort                  = { 320, 240 },
        .rateDistortionTradeOffPoints = { { VideoCodecEnum::kH264, { 640, 480 }, 500000 } },
        .maxContentBufferSize         = 4096,
        .microphoneCapabilities       = audioCaps,
        .spkrCapabilities             = audioCaps,
        .snapshotCapabilities         = { { { 640, 480 }, 30, ImageCodecEnum::kJpeg, false, MakeOptional(false) } },
        .maxNetworkBandwidth          = 10000000,
        .supportedStreamUsages        = { StreamUsageEnum::kLiveView, StreamUsageEnum::kRecording },
        .streamUsagePriorities        = { StreamUsageEnum::kLiveView, StreamUsageEnum::kRecording },
        .defaultAttributeValues       = { .viewport = { 0, 0, 1280, 720 } },
    };
}

struct TestStreamAllocator : public ::testing::Test
{
    static void SetUpTestSuite() { ASSERT_SUCCESS(Platform::MemoryInit()); }
    static void TearDownTestSuite() { Platform::MemoryShutdown(); }

    TestStreamAllocator() :
        mAllocator(mMediaControl, MakeAllocatorConfig(&mPrivacyObserver)), mCluster(MakeClusterArgs(mAllocator)), mTester(mCluster)
    {}

    void SetUp() override { ASSERT_SUCCESS(mCluster.Startup(mTester.GetServerClusterContext())); }
    void TearDown() override { mCluster.Shutdown(ClusterShutdownType::kClusterShutdown); }

    Commands::VideoStreamAllocate::Type MakeVideoAllocReq(VideoCodecEnum codec = VideoCodecEnum::kH264)
    {
        Commands::VideoStreamAllocate::Type req;
        req.streamUsage      = StreamUsageEnum::kLiveView;
        req.videoCodec       = codec;
        req.minFrameRate     = 15;
        req.maxFrameRate     = 30;
        req.minResolution    = { 640, 480 };
        req.maxResolution    = { 1280, 720 };
        req.minBitRate       = 500000;
        req.maxBitRate       = 2000000;
        req.keyFrameInterval = 1000;
        return req;
    }

    FakeMediaControl mMediaControl;
    RecordingPrivacyObserver mPrivacyObserver;
    StreamAllocator mAllocator;
    CameraAVStreamManagementCluster mCluster;
    ClusterTester mTester;
};

TEST_F(TestStreamAllocator, VideoAllocateReuseAndDeallocate)
{
    auto badReq = MakeVideoAllocReq(VideoCodecEnum::kHevc);
    EXPECT_EQ(mTester.Invoke(Commands::VideoStreamAllocate::Id, badReq).GetStatusCode(),
              ClusterStatusCode(Status::DynamicConstraintError));
    EXPECT_TRUE(mMediaControl.startedVideoStreams.empty());

    auto req  = MakeVideoAllocReq();
    auto res1 = mTester.Invoke(Commands::VideoStreamAllocate::Id, req);
    ASSERT_TRUE(res1.IsSuccess());
    ASSERT_TRUE(res1.response.has_value());
    uint16_t streamId = res1.response->videoStreamID;
    EXPECT_EQ(streamId, 1u);
    EXPECT_EQ(mMediaControl.startedVideoStreams.size(), 1u);
    EXPECT_EQ(mCluster.GetAllocatedVideoStreams().size(), 1u);

    // Reusable stream with narrower range triggers modification restart.
    auto reuseReq          = req;
    reuseReq.maxResolution = { 640, 480 };
    auto res2              = mTester.Invoke(Commands::VideoStreamAllocate::Id, reuseReq);
    ASSERT_TRUE(res2.IsSuccess());
    ASSERT_TRUE(res2.response.has_value());
    EXPECT_EQ(res2.response->videoStreamID, streamId);
    EXPECT_EQ(mCluster.GetAllocatedVideoStreams().size(), 1u);
    EXPECT_EQ(mMediaControl.stoppedVideoStreamIds.size(), 1u);
    EXPECT_EQ(mMediaControl.startedVideoStreams.size(), 2u);

    Commands::VideoStreamDeallocate::Type deallocReq;
    deallocReq.videoStreamID = streamId;
    EXPECT_TRUE(mTester.Invoke(Commands::VideoStreamDeallocate::Id, deallocReq).IsSuccess());
    EXPECT_TRUE(mCluster.GetAllocatedVideoStreams().empty());
    EXPECT_EQ(mMediaControl.stoppedVideoStreamIds.size(), 2u);
}

TEST_F(TestStreamAllocator, StartStreamErrorsPropagateAndDoNotCommit)
{
    mMediaControl.startVideoResult = CHIP_ERROR_INTERNAL;
    auto vRes                      = mTester.Invoke(Commands::VideoStreamAllocate::Id, MakeVideoAllocReq());
    EXPECT_EQ(vRes.GetStatusCode(), ClusterStatusCode(Status::Failure));
    EXPECT_TRUE(mCluster.GetAllocatedVideoStreams().empty());

    mMediaControl.startAudioResult = CHIP_ERROR_INTERNAL;
    Commands::AudioStreamAllocate::Type aReq;
    aReq.streamUsage  = StreamUsageEnum::kLiveView;
    aReq.audioCodec   = AudioCodecEnum::kOpus;
    aReq.channelCount = 1;
    aReq.sampleRate   = 48000;
    aReq.bitRate      = 32000;
    aReq.bitDepth     = 24;
    EXPECT_EQ(mTester.Invoke(Commands::AudioStreamAllocate::Id, aReq).GetStatusCode(), ClusterStatusCode(Status::Failure));
    EXPECT_TRUE(mCluster.GetAllocatedAudioStreams().empty());

    mMediaControl.startSnapshotResult = CHIP_ERROR_INTERNAL;
    Commands::SnapshotStreamAllocate::Type sReq;
    sReq.imageCodec    = ImageCodecEnum::kJpeg;
    sReq.maxFrameRate  = 30;
    sReq.minResolution = { 640, 480 };
    sReq.maxResolution = { 640, 480 };
    sReq.quality       = 80;
    EXPECT_EQ(mTester.Invoke(Commands::SnapshotStreamAllocate::Id, sReq).GetStatusCode(), ClusterStatusCode(Status::Failure));
    EXPECT_TRUE(mCluster.GetAllocatedSnapshotStreams().empty());
}

TEST_F(TestStreamAllocator, SnapshotAllocateCaptureAndDeallocate)
{
    Commands::SnapshotStreamAllocate::Type sReq;
    sReq.imageCodec    = ImageCodecEnum::kJpeg;
    sReq.maxFrameRate  = 30;
    sReq.minResolution = { 640, 480 };
    sReq.maxResolution = { 640, 480 };
    sReq.quality       = 80;
    auto allocRes      = mTester.Invoke(Commands::SnapshotStreamAllocate::Id, sReq);
    ASSERT_TRUE(allocRes.IsSuccess());
    ASSERT_TRUE(allocRes.response.has_value());
    uint16_t snapId = allocRes.response->snapshotStreamID;
    EXPECT_EQ(mMediaControl.startedSnapshotStreams.size(), 1u);

    Commands::CaptureSnapshot::Type capReq;
    capReq.snapshotStreamID.SetNonNull(snapId);
    capReq.requestedResolution = { 640, 480 };
    auto capRes                = mTester.Invoke(Commands::CaptureSnapshot::Id, capReq);
    ASSERT_TRUE(capRes.IsSuccess());
    ASSERT_TRUE(capRes.response.has_value());
    EXPECT_EQ(capRes.response->imageCodec, ImageCodecEnum::kJpeg);
    EXPECT_EQ(capRes.response->data.size(), 4u);
    ASSERT_EQ(mMediaControl.capturedStreamIds.size(), 1u);
    EXPECT_EQ(mMediaControl.capturedStreamIds[0], std::make_optional(snapId));

    capReq.snapshotStreamID.SetNull();
    EXPECT_TRUE(mTester.Invoke(Commands::CaptureSnapshot::Id, capReq).IsSuccess());
    ASSERT_EQ(mMediaControl.capturedStreamIds.size(), 2u);
    EXPECT_EQ(mMediaControl.capturedStreamIds[1], std::nullopt);

    mMediaControl.captureSnapshotResult = CHIP_ERROR_INTERNAL;
    EXPECT_EQ(mTester.Invoke(Commands::CaptureSnapshot::Id, capReq).GetStatusCode(), ClusterStatusCode(Status::Failure));

    Commands::SnapshotStreamDeallocate::Type deallocReq;
    deallocReq.snapshotStreamID = snapId;
    EXPECT_TRUE(mTester.Invoke(Commands::SnapshotStreamDeallocate::Id, deallocReq).IsSuccess());
    EXPECT_TRUE(mCluster.GetAllocatedSnapshotStreams().empty());
}

TEST_F(TestStreamAllocator, AcquireAndReleaseRefCountsWithRollback)
{
    auto vRes = mTester.Invoke(Commands::VideoStreamAllocate::Id, MakeVideoAllocReq());
    ASSERT_TRUE(vRes.IsSuccess());
    ASSERT_TRUE(vRes.response.has_value());
    uint16_t videoId = vRes.response->videoStreamID;

    Commands::AudioStreamAllocate::Type aReq;
    aReq.streamUsage  = StreamUsageEnum::kLiveView;
    aReq.audioCodec   = AudioCodecEnum::kOpus;
    aReq.channelCount = 1;
    aReq.sampleRate   = 48000;
    aReq.bitRate      = 32000;
    aReq.bitDepth     = 24;
    auto aRes         = mTester.Invoke(Commands::AudioStreamAllocate::Id, aReq);
    ASSERT_TRUE(aRes.IsSuccess());
    ASSERT_TRUE(aRes.response.has_value());
    uint16_t audioId = aRes.response->audioStreamID;

    // Partial failure in AcquireStreams must roll back audio refcount.
    EXPECT_EQ(mAllocator.AcquireStreams({ audioId }, { 999 }), CHIP_ERROR_NOT_FOUND);
    EXPECT_EQ(mCluster.GetAllocatedAudioStreams()[0].referenceCount, 0u);

    ASSERT_SUCCESS(mAllocator.AcquireStreams({ audioId }, { videoId }));
    EXPECT_EQ(mCluster.GetAllocatedAudioStreams()[0].referenceCount, 1u);
    EXPECT_EQ(mCluster.GetAllocatedVideoStreams()[0].referenceCount, 1u);

    Commands::VideoStreamDeallocate::Type deallocReq;
    deallocReq.videoStreamID = videoId;
    EXPECT_EQ(mTester.Invoke(Commands::VideoStreamDeallocate::Id, deallocReq).GetStatusCode(),
              ClusterStatusCode(Status::InvalidInState));

    ASSERT_SUCCESS(mAllocator.ReleaseStreams({ audioId }, { videoId }));
    EXPECT_EQ(mCluster.GetAllocatedAudioStreams()[0].referenceCount, 0u);
    EXPECT_EQ(mCluster.GetAllocatedVideoStreams()[0].referenceCount, 0u);
    EXPECT_TRUE(mTester.Invoke(Commands::VideoStreamDeallocate::Id, deallocReq).IsSuccess());
}

TEST_F(TestStreamAllocator, PrivacyObserverAndAttributeForwarding)
{
    EXPECT_TRUE(mTester.WriteAttribute(Attributes::SoftLivestreamPrivacyModeEnabled::Id, true).IsSuccess());
    EXPECT_EQ(mPrivacyObserver.softLivestreamCount, 1);
    EXPECT_TRUE(mPrivacyObserver.lastSoftLivestream);

    EXPECT_TRUE(mTester.WriteAttribute(Attributes::SoftRecordingPrivacyModeEnabled::Id, true).IsSuccess());
    EXPECT_EQ(mPrivacyObserver.softRecordingCount, 1);
    EXPECT_TRUE(mPrivacyObserver.lastSoftRecording);

    EXPECT_FALSE(mMediaControl.changedAttributes.empty());
}

TEST_F(TestStreamAllocator, PersistentStreamsRestartOnStartup)
{
    ASSERT_TRUE(mTester.Invoke(Commands::VideoStreamAllocate::Id, MakeVideoAllocReq()).IsSuccess());

    Commands::AudioStreamAllocate::Type aReq;
    aReq.streamUsage  = StreamUsageEnum::kLiveView;
    aReq.audioCodec   = AudioCodecEnum::kOpus;
    aReq.channelCount = 1;
    aReq.sampleRate   = 48000;
    aReq.bitRate      = 32000;
    aReq.bitDepth     = 24;
    ASSERT_TRUE(mTester.Invoke(Commands::AudioStreamAllocate::Id, aReq).IsSuccess());

    Commands::SnapshotStreamAllocate::Type sReq;
    sReq.imageCodec    = ImageCodecEnum::kJpeg;
    sReq.maxFrameRate  = 30;
    sReq.minResolution = { 640, 480 };
    sReq.maxResolution = { 640, 480 };
    sReq.quality       = 80;
    ASSERT_TRUE(mTester.Invoke(Commands::SnapshotStreamAllocate::Id, sReq).IsSuccess());

    mCluster.Shutdown(ClusterShutdownType::kClusterShutdown);

    FakeMediaControl restartedMedia;
    StreamAllocator restartedAllocator(restartedMedia, MakeAllocatorConfig());
    CameraAVStreamManagementCluster restartedCluster(MakeClusterArgs(restartedAllocator));
    ASSERT_SUCCESS(restartedCluster.Startup(mTester.GetServerClusterContext()));

    EXPECT_EQ(restartedMedia.startedVideoStreams.size(), 1u);
    EXPECT_EQ(restartedMedia.startedAudioStreams.size(), 1u);
    EXPECT_EQ(restartedMedia.startedSnapshotStreams.size(), 1u);

    restartedCluster.Shutdown(ClusterShutdownType::kClusterShutdown);
    // Restart original cluster so TearDown() shuts down cleanly.
    ASSERT_SUCCESS(mCluster.Startup(mTester.GetServerClusterContext()));
}

} // namespace
