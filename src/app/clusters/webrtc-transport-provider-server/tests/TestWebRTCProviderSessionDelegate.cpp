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

#include <app/CASESessionManager.h>
#include <app/clusters/camera-av-stream-management-server/CameraAVStreamManagementCluster.h>
#include <app/clusters/camera-av-stream-management-server/MediaControl.h>
#include <app/clusters/camera-av-stream-management-server/StreamAllocator.h>
#include <app/clusters/webrtc-transport-provider-server/WebRTCProviderSessionDelegate.h>
#include <app/clusters/webrtc-transport-provider-server/WebRTCTransportProviderCluster.h>
#include <app/server-cluster/testing/ClusterTester.h>
#include <clusters/CameraAvStreamManagement/Attributes.h>
#include <clusters/CameraAvStreamManagement/Commands.h>
#include <clusters/WebRTCTransportProvider/Commands.h>
#include <lib/core/CHIPError.h>
#include <lib/support/TimerDelegateMock.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::WebRTCTransportProvider;
using namespace chip::Testing;
using namespace chip::Protocols::InteractionModel;

constexpr EndpointId kTestEndpointId = 1;
constexpr const char kValidSdp[] =
    "v=0\r\no=- 1 2 IN IP4 127.0.0.1\r\ns=-\r\nc=IN IP4 127.0.0.1\r\nt=0 0\r\nm=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
    "a=ice-ufrag:ufrag\r\na=ice-pwd:password\r\na=fingerprint:sha-256 00:11\r\n";

class StubMediaControl : public CameraAvStreamManagement::MediaControl
{
public:
    CHIP_ERROR StartVideoStream(const CameraAvStreamManagement::VideoStreamStruct & stream) override { return CHIP_NO_ERROR; }
    CHIP_ERROR StopVideoStream(uint16_t streamId) override { return CHIP_NO_ERROR; }
    CHIP_ERROR StartAudioStream(const CameraAvStreamManagement::AudioStreamStruct & stream) override { return CHIP_NO_ERROR; }
    CHIP_ERROR StopAudioStream(uint16_t streamId) override { return CHIP_NO_ERROR; }
    CHIP_ERROR StartSnapshotStream(const CameraAvStreamManagement::SnapshotStreamStruct & stream) override { return CHIP_NO_ERROR; }
    CHIP_ERROR StopSnapshotStream(uint16_t streamId) override { return CHIP_NO_ERROR; }
    CHIP_ERROR CaptureSnapshot(std::optional<uint16_t> streamId, const CameraAvStreamManagement::VideoResolutionStruct & resolution,
                               CameraAvStreamManagement::ImageSnapshot & outSnapshot) override
    {
        return CHIP_NO_ERROR;
    }
};

class FakePeerConnectionFactory;

class FakePeerConnection : public webrtc::PeerConnection
{
public:
    FakePeerConnection(FakePeerConnectionFactory & factory, uint16_t sessionId, PeerConnection::Events & events);
    ~FakePeerConnection() override;

    CHIP_ERROR CreateOffer() override
    {
        createOfferCalls++;
        return createOfferResult;
    }

    CHIP_ERROR SetRemoteDescription(webrtc::SdpType type, CharSpan sdp) override
    {
        remoteDescriptions.emplace_back(type, std::string(sdp.data(), sdp.size()));
        return setRemoteResult;
    }

    CHIP_ERROR AddRemoteCandidate(const webrtc::IceCandidate & candidate) override
    {
        remoteCandidates.emplace_back(candidate.candidate.data(), candidate.candidate.size());
        return CHIP_NO_ERROR;
    }

    void Close() override { closed = true; }

    void EmitLocalDescription(webrtc::SdpType type, CharSpan sdp)
    {
        if (!closed)
        {
            events.OnLocalDescription(type, sdp);
        }
    }

    void EmitLocalCandidate(CharSpan candidate, std::optional<CharSpan> mid = std::nullopt,
                            std::optional<uint16_t> mline = std::nullopt)
    {
        if (!closed)
        {
            events.OnLocalCandidate(webrtc::IceCandidate{ candidate, mid, mline });
        }
    }

    void EmitConnected()
    {
        if (!closed)
        {
            events.OnConnected();
        }
    }

    void EmitClosed()
    {
        if (!closed)
        {
            events.OnClosed();
        }
    }

    FakePeerConnectionFactory & factory;
    uint16_t sessionId = 0;
    PeerConnection::Events & events;
    CHIP_ERROR createOfferResult = CHIP_NO_ERROR;
    CHIP_ERROR setRemoteResult   = CHIP_NO_ERROR;
    int createOfferCalls         = 0;
    bool closed                  = false;
    std::vector<std::pair<webrtc::SdpType, std::string>> remoteDescriptions;
    std::vector<std::string> remoteCandidates;
};

class FakePeerConnectionFactory : public webrtc::PeerConnectionFactory
{
public:
    struct CreatedRecord
    {
        uint16_t sessionId;
        std::vector<uint16_t> videoStreamIds;
        std::vector<uint16_t> audioStreamIds;
        std::optional<webrtc::IceTransportPolicy> iceTransportPolicy;
        std::optional<uint16_t> sframeCipherSuite;
        size_t iceServerCount = 0;
    };

    std::unique_ptr<webrtc::PeerConnection> Create(const SessionConfig & config, webrtc::PeerConnection::Events & events) override
    {
        CreatedRecord rec;
        rec.sessionId          = config.sessionId;
        rec.iceTransportPolicy = config.iceTransportPolicy;
        rec.iceServerCount     = config.iceServers.size();
        if (config.sFrameConfig.has_value())
        {
            rec.sframeCipherSuite = config.sFrameConfig->cipherSuite;
        }
        rec.videoStreamIds.assign(config.videoStreamIds.begin(), config.videoStreamIds.end());
        rec.audioStreamIds.assign(config.audioStreamIds.begin(), config.audioStreamIds.end());
        createdRecords.push_back(std::move(rec));

        auto conn = std::make_unique<FakePeerConnection>(*this, config.sessionId, events);
        activeConnections.push_back(conn.get());
        return conn;
    }

    FakePeerConnection * FindConnection(uint16_t sessionId) const
    {
        for (auto * conn : activeConnections)
        {
            if (conn->sessionId == sessionId)
            {
                return conn;
            }
        }
        return nullptr;
    }

    std::vector<CreatedRecord> createdRecords;
    std::vector<FakePeerConnection *> activeConnections;
    int destroyedCount = 0;
};

FakePeerConnection::FakePeerConnection(FakePeerConnectionFactory & factoryRef, uint16_t id, PeerConnection::Events & ev) :
    factory(factoryRef), sessionId(id), events(ev)
{}

FakePeerConnection::~FakePeerConnection()
{
    factory.destroyedCount++;
    auto & list = factory.activeConnections;
    list.erase(std::remove(list.begin(), list.end(), this), list.end());
}

class InterceptingRequestorClient : public webrtc::WebRTCTransportRequestorClient
{
public:
    using WebRTCTransportRequestorClient::CommandType;
    using WebRTCTransportRequestorClient::WebRTCTransportRequestorClient;

    struct CommandRecord
    {
        CommandType type;
        uint16_t sessionId = 0;
        std::string sdp;
        std::vector<std::string> candidates;
        Clusters::Globals::WebRTCEndReasonEnum endReason = Clusters::Globals::WebRTCEndReasonEnum::kUnknownEnumValue;
    };

    void EstablishSession(PendingRequest & request) override
    {
        CommandRecord rec;
        rec.type      = request.commandType;
        rec.sessionId = request.sessionId;
        rec.sdp       = request.sdp;
        rec.endReason = request.endReason;
        for (const auto & c : request.candidates)
        {
            rec.candidates.push_back(c.candidate);
        }
        sentCommands.push_back(std::move(rec));

        auto owned = TakeRequest(&request);
        if (owned && !owned->cancelled && owned->callback != nullptr)
        {
            owned->callback->OnCommandResult(owned->sessionId, CHIP_NO_ERROR, Status::Success);
        }
    }

    std::vector<CommandRecord> sentCommands;
};

CameraAvStreamManagement::StreamAllocator::Config MakeAllocatorConfig()
{
    CameraAvStreamManagement::StreamAllocator::Config config;
    config.availableVideoStreams = {
        { 10,
          StreamUsageEnum::kLiveView,
          CameraAvStreamManagement::VideoCodecEnum::kH264,
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
        { 20, StreamUsageEnum::kLiveView, CameraAvStreamManagement::AudioCodecEnum::kOpus, 2, 48000, 64000, 24, 0 },
    };
    return config;
}

CameraAvStreamManagement::CameraAVStreamManagementCluster::InitArguments
MakeAvsmArgs(CameraAvStreamManagement::CameraAVStreamManagementDelegate & delegate)
{
    static std::array<CameraAvStreamManagement::AudioCodecEnum, 1> audioCodecs = {
        CameraAvStreamManagement::AudioCodecEnum::kOpus
    };
    static std::array<uint32_t, 1> sampleRates                         = { 48000 };
    static std::array<uint8_t, 1> bitDepths                            = { 24 };
    static CameraAvStreamManagement::AudioCapabilitiesStruct audioCaps = {
        2, Span<CameraAvStreamManagement::AudioCodecEnum>(audioCodecs), Span<uint32_t>(sampleRates), Span<uint8_t>(bitDepths)
    };

    return CameraAvStreamManagement::CameraAVStreamManagementCluster::InitArguments{
        .delegate   = delegate,
        .endpointId = kTestEndpointId,
        .features   = BitFlags<CameraAvStreamManagement::Feature>(CameraAvStreamManagement::Feature::kVideo,
                                                                  CameraAvStreamManagement::Feature::kAudio,
                                                                  CameraAvStreamManagement::Feature::kPrivacy),
        .optionalAttrs =
            BitFlags<CameraAvStreamManagement::OptionalAttribute>(CameraAvStreamManagement::OptionalAttribute::kHardPrivacyModeOn),
        .maxConcurrentEncoders        = 2,
        .maxEncodedPixelRate          = 1280 * 720 * 60,
        .videoSensorParams            = { 1280, 720, 60, Optional<uint16_t>() },
        .minViewPort                  = { 320, 240 },
        .rateDistortionTradeOffPoints = { { CameraAvStreamManagement::VideoCodecEnum::kH264, { 640, 480 }, 500000 } },
        .maxContentBufferSize         = 4096,
        .microphoneCapabilities       = audioCaps,
        .spkrCapabilities             = audioCaps,
        .maxNetworkBandwidth          = 10000000,
        .supportedStreamUsages        = { StreamUsageEnum::kLiveView, StreamUsageEnum::kRecording },
        .streamUsagePriorities        = { StreamUsageEnum::kLiveView, StreamUsageEnum::kRecording },
        .defaultAttributeValues       = { .viewport = { 0, 0, 1280, 720 } },
    };
}

struct TestWebRTCProviderSessionDelegate : public ::testing::Test
{
    static void SetUpTestSuite() { ASSERT_SUCCESS(Platform::MemoryInit()); }
    static void TearDownTestSuite() { Platform::MemoryShutdown(); }

    TestWebRTCProviderSessionDelegate() :
        mAllocator(mMediaControl, MakeAllocatorConfig()), mAvsmCluster(MakeAvsmArgs(mAllocator)), mAvsmTester(mAvsmCluster),
        mRequestorClient(mCaseSessionManager), mSessionDelegate(WebRTCProviderSessionDelegate::Context{
                                                   .peerFactory              = mPeerFactory,
                                                   .requestorClient          = mRequestorClient,
                                                   .avsmCluster              = mAvsmCluster,
                                                   .timerDelegate            = mTimerDelegate,
                                                   .isUtcTimeNull            = [this]() { return mUtcTimeIsNull; },
                                                   .maxConcurrentSessions    = 2,
                                                   .connectionTimeoutSeconds = 30,
                                                   .deferredOffer            = false,
                                               }),
        mProviderCluster(kTestEndpointId, mSessionDelegate, mFabricTable), mProviderTester(mProviderCluster)
    {
        mSessionDelegate.SetWebRTCTransportProvider(&mProviderCluster);
        mAllocator.SetPrivacyObserver(&mSessionDelegate);
    }

    void SetUp() override
    {
        ASSERT_SUCCESS(mAvsmCluster.Startup(mAvsmTester.GetServerClusterContext()));
        ASSERT_SUCCESS(mProviderCluster.Startup(mProviderTester.GetServerClusterContext()));

        CameraAvStreamManagement::Commands::VideoStreamAllocate::Type vReq;
        vReq.streamUsage      = StreamUsageEnum::kLiveView;
        vReq.videoCodec       = CameraAvStreamManagement::VideoCodecEnum::kH264;
        vReq.minFrameRate     = 15;
        vReq.maxFrameRate     = 30;
        vReq.minResolution    = { 640, 480 };
        vReq.maxResolution    = { 1280, 720 };
        vReq.minBitRate       = 500000;
        vReq.maxBitRate       = 2000000;
        vReq.keyFrameInterval = 1000;
        auto vRes             = mAvsmTester.Invoke(CameraAvStreamManagement::Commands::VideoStreamAllocate::Id, vReq);
        ASSERT_TRUE(vRes.IsSuccess());
        ASSERT_TRUE(vRes.response.has_value());
        mVideoStreamId = vRes.response->videoStreamID;

        CameraAvStreamManagement::Commands::AudioStreamAllocate::Type aReq;
        aReq.streamUsage  = StreamUsageEnum::kLiveView;
        aReq.audioCodec   = CameraAvStreamManagement::AudioCodecEnum::kOpus;
        aReq.channelCount = 1;
        aReq.sampleRate   = 48000;
        aReq.bitRate      = 32000;
        aReq.bitDepth     = 24;
        auto aRes         = mAvsmTester.Invoke(CameraAvStreamManagement::Commands::AudioStreamAllocate::Id, aReq);
        ASSERT_TRUE(aRes.IsSuccess());
        ASSERT_TRUE(aRes.response.has_value());
        mAudioStreamId = aRes.response->audioStreamID;
    }

    void TearDown() override
    {
        mSessionDelegate.Shutdown();
        mProviderCluster.Shutdown(ClusterShutdownType::kClusterShutdown);
        mAvsmCluster.Shutdown(ClusterShutdownType::kClusterShutdown);
    }

    StubMediaControl mMediaControl;
    CameraAvStreamManagement::StreamAllocator mAllocator;
    CameraAvStreamManagement::CameraAVStreamManagementCluster mAvsmCluster;
    ClusterTester mAvsmTester;
    CASESessionManager mCaseSessionManager;
    FakePeerConnectionFactory mPeerFactory;
    InterceptingRequestorClient mRequestorClient;
    TimerDelegateMock mTimerDelegate;
    FabricTable mFabricTable;
    bool mUtcTimeIsNull = false;
    WebRTCProviderSessionDelegate mSessionDelegate;
    WebRTCTransportProviderCluster mProviderCluster;
    ClusterTester mProviderTester;
    uint16_t mVideoStreamId = 0;
    uint16_t mAudioStreamId = 0;
};

TEST_F(TestWebRTCProviderSessionDelegate, SolicitOfferFullNegotiationAndEndSession)
{
    Commands::SolicitOffer::Type solicit;
    solicit.streamUsage           = StreamUsageEnum::kLiveView;
    solicit.originatingEndpointID = 2;
    solicit.videoStreamID.SetValue(DataModel::Nullable<uint16_t>()); // auto-select
    solicit.audioStreamID.SetValue(DataModel::Nullable<uint16_t>()); // auto-select
    solicit.ICETransportPolicy = MakeOptional("relay"_span);

    auto res = mProviderTester.Invoke(Commands::SolicitOffer::Id, solicit);
    ASSERT_TRUE(res.IsSuccess());
    ASSERT_TRUE(res.response.has_value());
    uint16_t sessionId = res.response->webRTCSessionID;

    EXPECT_EQ(mSessionDelegate.SessionCount(), 1u);
    EXPECT_EQ(mAvsmCluster.GetAllocatedVideoStreams()[0].referenceCount, 1u);
    EXPECT_EQ(mAvsmCluster.GetAllocatedAudioStreams()[0].referenceCount, 1u);

    auto sessions = mProviderCluster.GetCurrentSessions();
    ASSERT_EQ(sessions.size(), 1u);
    ASSERT_TRUE(sessions[0].videoStreams.HasValue());
    ASSERT_TRUE(sessions[0].audioStreams.HasValue());
    EXPECT_EQ(*sessions[0].videoStreams.Value().begin(), mVideoStreamId);
    EXPECT_EQ(*sessions[0].audioStreams.Value().begin(), mAudioStreamId);

    ASSERT_EQ(mPeerFactory.createdRecords.size(), 1u);
    EXPECT_EQ(mPeerFactory.createdRecords[0].iceTransportPolicy, std::make_optional(webrtc::IceTransportPolicy::kRelay));
    EXPECT_EQ(mPeerFactory.createdRecords[0].videoStreamIds, (std::vector<uint16_t>{ mVideoStreamId }));
    EXPECT_EQ(mPeerFactory.createdRecords[0].audioStreamIds, (std::vector<uint16_t>{ mAudioStreamId }));

    FakePeerConnection * conn = mPeerFactory.FindConnection(sessionId);
    ASSERT_NE(conn, nullptr);
    EXPECT_EQ(conn->createOfferCalls, 1);

    // Candidate emitted before local Offer must wait until Offer is sent, then flush.
    conn->EmitLocalCandidate("early-cand"_span, "0"_span, 0);
    EXPECT_TRUE(mRequestorClient.sentCommands.empty());

    conn->EmitLocalDescription(webrtc::SdpType::kOffer, CharSpan::fromCharString(kValidSdp));
    ASSERT_EQ(mRequestorClient.sentCommands.size(), 2u);
    EXPECT_EQ(mRequestorClient.sentCommands[0].type, InterceptingRequestorClient::CommandType::kOffer);
    EXPECT_EQ(mRequestorClient.sentCommands[1].type, InterceptingRequestorClient::CommandType::kICECandidates);
    EXPECT_EQ(mRequestorClient.sentCommands[1].candidates[0], "early-cand");

    Commands::ProvideAnswer::Type answer;
    answer.webRTCSessionID = sessionId;
    answer.sdp             = CharSpan::fromCharString(kValidSdp);
    EXPECT_TRUE(mProviderTester.Invoke(Commands::ProvideAnswer::Id, answer).IsSuccess());
    ASSERT_EQ(conn->remoteDescriptions.size(), 1u);
    EXPECT_EQ(conn->remoteDescriptions[0].first, webrtc::SdpType::kAnswer);

    // Connected cancels timeout timer; advancing past 30s keeps session alive.
    conn->EmitConnected();
    mTimerDelegate.AdvanceClock(System::Clock::Seconds32(35));
    EXPECT_EQ(mSessionDelegate.SessionCount(), 1u);

    Commands::EndSession::Type endReq;
    endReq.webRTCSessionID = sessionId;
    endReq.reason          = WebRTCEndReasonEnum::kUserHangup;
    EXPECT_TRUE(mProviderTester.Invoke(Commands::EndSession::Id, endReq).IsSuccess());

    EXPECT_EQ(mSessionDelegate.SessionCount(), 0u);
    EXPECT_TRUE(mProviderCluster.GetCurrentSessions().empty());
    EXPECT_EQ(mAvsmCluster.GetAllocatedVideoStreams()[0].referenceCount, 0u);
    EXPECT_EQ(mAvsmCluster.GetAllocatedAudioStreams()[0].referenceCount, 0u);
}

TEST_F(TestWebRTCProviderSessionDelegate, ProvideOfferSlotLimitDoesNotLeak)
{
    Commands::ProvideOffer::Type offer;
    offer.webRTCSessionID.SetNull();
    offer.sdp                   = CharSpan::fromCharString(kValidSdp);
    offer.streamUsage           = StreamUsageEnum::kLiveView;
    offer.originatingEndpointID = 2;
    offer.videoStreamID.SetValue(DataModel::Nullable<uint16_t>(mVideoStreamId));

    ASSERT_TRUE(mProviderTester.Invoke(Commands::ProvideOffer::Id, offer).IsSuccess());
    ASSERT_TRUE(mProviderTester.Invoke(Commands::ProvideOffer::Id, offer).IsSuccess());
    EXPECT_EQ(mSessionDelegate.SessionCount(), 2u);
    EXPECT_EQ(mProviderCluster.GetCurrentSessions().size(), 2u);

    auto overLimit = mProviderTester.Invoke(Commands::ProvideOffer::Id, offer);
    EXPECT_EQ(overLimit.GetStatusCode(), ClusterStatusCode(Status::ResourceExhausted));
    EXPECT_EQ(mSessionDelegate.SessionCount(), 2u);
    EXPECT_EQ(mProviderCluster.GetCurrentSessions().size(), 2u);
    EXPECT_EQ(mAvsmCluster.GetAllocatedVideoStreams()[0].referenceCount, 2u);

    // Ending one frees a slot for a subsequent ProvideOffer.
    uint16_t firstId = mProviderCluster.GetCurrentSessions()[0].id;
    Commands::EndSession::Type endReq;
    endReq.webRTCSessionID = firstId;
    endReq.reason          = WebRTCEndReasonEnum::kUserHangup;
    ASSERT_TRUE(mProviderTester.Invoke(Commands::EndSession::Id, endReq).IsSuccess());
    EXPECT_TRUE(mProviderTester.Invoke(Commands::ProvideOffer::Id, offer).IsSuccess());
    EXPECT_EQ(mSessionDelegate.SessionCount(), 2u);
}

TEST_F(TestWebRTCProviderSessionDelegate, SolicitOfferOverLimitSendsEndAndRemovesClusterSession)
{
    Commands::ProvideOffer::Type offer;
    offer.webRTCSessionID.SetNull();
    offer.sdp                   = CharSpan::fromCharString(kValidSdp);
    offer.streamUsage           = StreamUsageEnum::kLiveView;
    offer.originatingEndpointID = 2;
    offer.videoStreamID.SetValue(DataModel::Nullable<uint16_t>(mVideoStreamId));
    ASSERT_TRUE(mProviderTester.Invoke(Commands::ProvideOffer::Id, offer).IsSuccess());
    ASSERT_TRUE(mProviderTester.Invoke(Commands::ProvideOffer::Id, offer).IsSuccess());

    Commands::SolicitOffer::Type solicit;
    solicit.streamUsage           = StreamUsageEnum::kLiveView;
    solicit.originatingEndpointID = 2;
    solicit.videoStreamID.SetValue(DataModel::Nullable<uint16_t>(mVideoStreamId));
    auto res = mProviderTester.Invoke(Commands::SolicitOffer::Id, solicit);
    ASSERT_TRUE(res.IsSuccess());
    ASSERT_TRUE(res.response.has_value());
    uint16_t overId = res.response->webRTCSessionID;
    EXPECT_EQ(mProviderCluster.GetCurrentSessions().size(), 3u);

    mTimerDelegate.AdvanceClock(System::Clock::kZero);
    EXPECT_EQ(mProviderCluster.GetCurrentSessions().size(), 2u);
    EXPECT_FALSE(mSessionDelegate.HasSession(overId));
    ASSERT_FALSE(mRequestorClient.sentCommands.empty());
    EXPECT_EQ(mRequestorClient.sentCommands.back().type, InterceptingRequestorClient::CommandType::kEnd);
    EXPECT_EQ(mRequestorClient.sentCommands.back().sessionId, overId);
    EXPECT_EQ(mRequestorClient.sentCommands.back().endReason, WebRTCEndReasonEnum::kOutOfResources);
}

TEST_F(TestWebRTCProviderSessionDelegate, PeerClosedAndTimeoutCleanUpClusterState)
{
    Commands::ProvideOffer::Type offer;
    offer.webRTCSessionID.SetNull();
    offer.sdp                   = CharSpan::fromCharString(kValidSdp);
    offer.streamUsage           = StreamUsageEnum::kLiveView;
    offer.originatingEndpointID = 2;
    offer.videoStreamID.SetValue(DataModel::Nullable<uint16_t>(mVideoStreamId));

    auto res1 = mProviderTester.Invoke(Commands::ProvideOffer::Id, offer);
    ASSERT_TRUE(res1.IsSuccess());
    ASSERT_TRUE(res1.response.has_value());
    uint16_t id1 = res1.response->webRTCSessionID;

    FakePeerConnection * conn1 = mPeerFactory.FindConnection(id1);
    ASSERT_NE(conn1, nullptr);
    conn1->EmitClosed();
    EXPECT_EQ(mSessionDelegate.SessionCount(), 0u);
    EXPECT_TRUE(mProviderCluster.GetCurrentSessions().empty());
    EXPECT_EQ(mAvsmCluster.GetAllocatedVideoStreams()[0].referenceCount, 0u);

    auto res2 = mProviderTester.Invoke(Commands::ProvideOffer::Id, offer);
    ASSERT_TRUE(res2.IsSuccess());
    ASSERT_TRUE(res2.response.has_value());
    EXPECT_EQ(mSessionDelegate.SessionCount(), 1u);

    mTimerDelegate.AdvanceClock(System::Clock::Seconds32(31));
    EXPECT_EQ(mSessionDelegate.SessionCount(), 0u);
    EXPECT_TRUE(mProviderCluster.GetCurrentSessions().empty());
    EXPECT_EQ(mAvsmCluster.GetAllocatedVideoStreams()[0].referenceCount, 0u);
}

TEST_F(TestWebRTCProviderSessionDelegate, PrivacyModeEndsLiveViewSession)
{
    Commands::ProvideOffer::Type offer;
    offer.webRTCSessionID.SetNull();
    offer.sdp                   = CharSpan::fromCharString(kValidSdp);
    offer.streamUsage           = StreamUsageEnum::kLiveView;
    offer.originatingEndpointID = 2;
    offer.videoStreamID.SetValue(DataModel::Nullable<uint16_t>(mVideoStreamId));
    offer.audioStreamID.SetValue(DataModel::Nullable<uint16_t>(mAudioStreamId));

    auto res = mProviderTester.Invoke(Commands::ProvideOffer::Id, offer);
    ASSERT_TRUE(res.IsSuccess());
    ASSERT_TRUE(res.response.has_value());
    uint16_t sessionId = res.response->webRTCSessionID;

    EXPECT_TRUE(
        mAvsmTester.WriteAttribute(CameraAvStreamManagement::Attributes::SoftLivestreamPrivacyModeEnabled::Id, true).IsSuccess());
    EXPECT_EQ(mSessionDelegate.SessionCount(), 0u);
    EXPECT_TRUE(mProviderCluster.GetCurrentSessions().empty());
    EXPECT_EQ(mAvsmCluster.GetAllocatedVideoStreams()[0].referenceCount, 0u);
    EXPECT_EQ(mAvsmCluster.GetAllocatedAudioStreams()[0].referenceCount, 0u);

    ASSERT_FALSE(mRequestorClient.sentCommands.empty());
    EXPECT_EQ(mRequestorClient.sentCommands.back().type, InterceptingRequestorClient::CommandType::kEnd);
    EXPECT_EQ(mRequestorClient.sentCommands.back().sessionId, sessionId);
    EXPECT_EQ(mRequestorClient.sentCommands.back().endReason, WebRTCEndReasonEnum::kPrivacyMode);
}

TEST_F(TestWebRTCProviderSessionDelegate, SFrameAndUtcTimeValidation)
{
    uint8_t shortKey[8] = {};
    Structs::SFrameStruct::Type badSframe;
    badSframe.cipherSuite = 0x0001; // expects 16 bytes
    badSframe.baseKey     = ByteSpan(shortKey, sizeof(shortKey));
    const uint8_t kid[2]  = { 0x01, 0x02 };
    badSframe.kid         = ByteSpan(kid, sizeof(kid));

    Commands::ProvideOffer::Type offer;
    offer.webRTCSessionID.SetNull();
    offer.sdp                   = CharSpan::fromCharString(kValidSdp);
    offer.streamUsage           = StreamUsageEnum::kLiveView;
    offer.originatingEndpointID = 2;
    offer.videoStreamID.SetValue(DataModel::Nullable<uint16_t>(mVideoStreamId));
    offer.SFrameConfig = MakeOptional(badSframe);

    EXPECT_EQ(mProviderTester.Invoke(Commands::ProvideOffer::Id, offer).GetStatusCode(),
              ClusterStatusCode(Status::DynamicConstraintError));

    offer.SFrameConfig.ClearValue();
    mUtcTimeIsNull = true;

    const CharSpan urls[] = { "turns:turn.example.com:5349"_span };
    Globals::Structs::ICEServerStruct::Type iceServer;
    iceServer.URLs   = DataModel::List<const CharSpan>(urls, 1);
    offer.ICEServers = MakeOptional(DataModel::List<const Globals::Structs::ICEServerStruct::Type>(&iceServer, 1));

    EXPECT_EQ(mProviderTester.Invoke(Commands::ProvideOffer::Id, offer).GetStatusCode(), ClusterStatusCode(Status::InvalidInState));
}

} // namespace
