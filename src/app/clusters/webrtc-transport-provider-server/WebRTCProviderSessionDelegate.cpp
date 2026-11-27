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

#include <app/clusters/webrtc-transport-provider-server/WebRTCProviderSessionDelegate.h>

#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

#include <algorithm>
#include <cctype>
#include <string_view>

namespace chip {
namespace app {
namespace Clusters {
namespace WebRTCTransportProvider {

namespace {

constexpr uint16_t kCipherSuiteAes128Gcm = 0x0001;
constexpr uint16_t kCipherSuiteAes256Gcm = 0x0002;
constexpr size_t kAes128KeyLength        = 16;
constexpr size_t kAes256KeyLength        = 32;

bool ValidateSdpFields(std::string_view sdp)
{
    if (sdp.empty())
    {
        ChipLogError(Camera, "WebRTCProviderSessionDelegate: SDP is empty");
        return false;
    }

    static constexpr std::string_view kRequiredFields[] = {
        "v=", "o=", "s=", "c=", "t=", "m=", "a=ice-ufrag:", "a=ice-pwd:", "a=fingerprint:",
    };

    for (std::string_view field : kRequiredFields)
    {
        if (sdp.find(field) == std::string_view::npos)
        {
            ChipLogError(Camera, "WebRTCProviderSessionDelegate: SDP missing required field %.*s", static_cast<int>(field.size()),
                         field.data());
            return false;
        }
    }

    return true;
}

std::optional<webrtc::IceTransportPolicy> ParseIceTransportPolicy(const Optional<std::string> & policyOpt)
{
    if (!policyOpt.HasValue())
    {
        return std::nullopt;
    }

    std::string lower = policyOpt.Value();
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "relay")
    {
        return webrtc::IceTransportPolicy::kRelay;
    }
    if (lower == "all")
    {
        return webrtc::IceTransportPolicy::kAll;
    }
    return std::nullopt;
}

struct ConvertedIceServers
{
    struct ServerStorage
    {
        std::vector<std::string> urls;
        std::vector<CharSpan> urlSpans;
        std::string username;
        std::string credential;
    };

    std::vector<ServerStorage> storage;
    std::vector<webrtc::IceServer> servers;

    CHIP_ERROR Init(const Optional<std::vector<ICEServerDecodableStruct>> & matterServers)
    {
        if (!matterServers.HasValue())
        {
            return CHIP_NO_ERROR;
        }

        storage.reserve(matterServers.Value().size());
        for (const auto & src : matterServers.Value())
        {
            ServerStorage & dst = storage.emplace_back();
            auto urlsIter       = src.URLs.begin();
            while (urlsIter.Next())
            {
                dst.urls.emplace_back(urlsIter.GetValue().data(), urlsIter.GetValue().size());
            }
            ReturnErrorOnFailure(urlsIter.GetStatus());

            if (dst.urls.empty())
            {
                storage.pop_back();
                continue;
            }

            if (src.username.HasValue())
            {
                dst.username.assign(src.username.Value().data(), src.username.Value().size());
            }
            if (src.credential.HasValue())
            {
                dst.credential.assign(src.credential.Value().data(), src.credential.Value().size());
            }
        }

        servers.reserve(storage.size());
        for (auto & dst : storage)
        {
            dst.urlSpans.reserve(dst.urls.size());
            for (const auto & url : dst.urls)
            {
                dst.urlSpans.emplace_back(url.data(), url.size());
            }
            servers.push_back(webrtc::IceServer{
                .urls       = Span<const CharSpan>(dst.urlSpans.data(), dst.urlSpans.size()),
                .username   = CharSpan(dst.username.data(), dst.username.size()),
                .credential = CharSpan(dst.credential.data(), dst.credential.size()),
            });
        }

        return CHIP_NO_ERROR;
    }
};

template <typename StreamStruct, typename IdGetter>
CHIP_ERROR ValidateStreamsExist(const std::vector<uint16_t> & requestedIds, const std::vector<StreamStruct> & allocatedStreams,
                                IdGetter getId)
{
    for (uint16_t streamId : requestedIds)
    {
        bool found = std::any_of(allocatedStreams.begin(), allocatedStreams.end(),
                                 [&](const StreamStruct & stream) { return getId(stream) == streamId; });
        if (!found)
        {
            return CHIP_ERROR_INVALID_ARGUMENT;
        }
    }
    return CHIP_NO_ERROR;
}

} // namespace

WebRTCProviderSessionDelegate::WebRTCProviderSessionDelegate(Context context) : mContext(std::move(context)) {}

WebRTCProviderSessionDelegate::~WebRTCProviderSessionDelegate()
{
    Shutdown();
}

void WebRTCProviderSessionDelegate::Shutdown()
{
    std::vector<uint16_t> sessionIds;
    sessionIds.reserve(mSessions.size());
    for (const auto & [id, session] : mSessions)
    {
        sessionIds.push_back(id);
    }

    for (uint16_t sessionId : sessionIds)
    {
        CleanupSession(sessionId);
    }
}

WebRTCProviderSessionDelegate::Session * WebRTCProviderSessionDelegate::FindSession(uint16_t sessionId)
{
    auto it = mSessions.find(sessionId);
    return (it != mSessions.end()) ? it->second.get() : nullptr;
}

void WebRTCProviderSessionDelegate::PopulateOutSession(const OfferRequestArgs & args, const Session & session,
                                                       WebRTCSessionStruct & outSession)
{
    outSession.id             = args.sessionId;
    outSession.peerNodeID     = args.peerNodeId;
    outSession.peerEndpointID = args.originatingEndpointId;
    outSession.streamUsage    = args.streamUsage;
    outSession.SetFabricIndex(args.fabricIndex);

    if (!session.videoStreams.empty())
    {
        outSession.videoStreamID.SetNonNull(session.videoStreams[0]);
        outSession.videoStreams.SetValue(DataModel::List<const uint16_t>(session.videoStreams.data(), session.videoStreams.size()));
    }
    else
    {
        outSession.videoStreamID.SetNull();
        outSession.videoStreams.ClearValue();
    }

    if (!session.audioStreams.empty())
    {
        outSession.audioStreamID.SetNonNull(session.audioStreams[0]);
        outSession.audioStreams.SetValue(DataModel::List<const uint16_t>(session.audioStreams.data(), session.audioStreams.size()));
    }
    else
    {
        outSession.audioStreamID.SetNull();
        outSession.audioStreams.ClearValue();
    }
}

CHIP_ERROR WebRTCProviderSessionDelegate::HandleSolicitOffer(const OfferRequestArgs & args, WebRTCSessionStruct & outSession,
                                                             bool & outDeferredOffer)
{
    outDeferredOffer = mContext.deferredOffer;

    auto session                   = std::make_unique<Session>(*this, args.sessionId);
    session->peer                  = ScopedNodeId(args.peerNodeId, args.fabricIndex);
    session->originatingEndpointId = args.originatingEndpointId;
    session->streamUsage           = args.streamUsage;
    if (args.videoStreams.HasValue())
    {
        session->videoStreams = args.videoStreams.Value();
    }
    if (args.audioStreams.HasValue())
    {
        session->audioStreams = args.audioStreams.Value();
    }

    if (mSessions.size() >= mContext.maxConcurrentSessions)
    {
        ChipLogProgress(Camera, "WebRTCProviderSessionDelegate: max concurrent sessions (%u) reached on SolicitOffer",
                        mContext.maxConcurrentSessions);
        session->state            = SessionState::kPendingEnd;
        session->pendingEndReason = WebRTCEndReasonEnum::kOutOfResources;

        PopulateOutSession(args, *session, outSession);

        Session * rawSession      = session.get();
        mSessions[args.sessionId] = std::move(session);
        LogErrorOnFailure(mContext.timerDelegate.StartTimer(&rawSession->pendingEndTimer, System::Clock::kZero));
        return CHIP_NO_ERROR;
    }

    ConvertedIceServers iceServers;
    ReturnErrorOnFailure(iceServers.Init(args.iceServers));

    std::optional<webrtc::SFrameConfig> sFrameConfig;
    if (args.sFrameConfig.HasValue())
    {
        sFrameConfig = webrtc::SFrameConfig{
            .cipherSuite = args.sFrameConfig.Value().cipherSuite,
            .baseKey     = args.sFrameConfig.Value().baseKey,
            .kid         = args.sFrameConfig.Value().kid,
        };
    }

    webrtc::PeerConnectionFactory::SessionConfig config{
        .sessionId          = args.sessionId,
        .iceServers         = Span<const webrtc::IceServer>(iceServers.servers.data(), iceServers.servers.size()),
        .iceTransportPolicy = ParseIceTransportPolicy(args.iceTransportPolicy),
        .sFrameConfig       = sFrameConfig,
        .videoStreamIds     = Span<const uint16_t>(session->videoStreams.data(), session->videoStreams.size()),
        .audioStreamIds     = Span<const uint16_t>(session->audioStreams.data(), session->audioStreams.size()),
    };

    session->peerConnection = mContext.peerFactory.Create(config, session->events);
    VerifyOrReturnError(session->peerConnection != nullptr, CHIP_ERROR_NO_MEMORY);

    CHIP_ERROR err = AcquireStreams(session->audioStreams, session->videoStreams);
    if (err != CHIP_NO_ERROR)
    {
        session->peerConnection->Close();
        session->peerConnection.reset();
        return err;
    }
    session->streamsAcquired = true;

    PopulateOutSession(args, *session, outSession);

    Session * rawSession      = session.get();
    mSessions[args.sessionId] = std::move(session);
    rawSession->state         = SessionState::kWaitingLocalOffer;

    if (mContext.connectionTimeoutSeconds > 0)
    {
        LogErrorOnFailure(mContext.timerDelegate.StartTimer(&rawSession->timeoutTimer,
                                                            System::Clock::Seconds32(mContext.connectionTimeoutSeconds)));
    }

    err = rawSession->peerConnection->CreateOffer();
    if (err != CHIP_NO_ERROR)
    {
        CleanupSession(args.sessionId);
        return err;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR WebRTCProviderSessionDelegate::HandleProvideOffer(const ProvideOfferRequestArgs & args, WebRTCSessionStruct & outSession)
{
    if (!ValidateSdpFields(std::string_view(args.sdp.data(), args.sdp.size())))
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    Session * existing = FindSession(args.sessionId);
    if (existing != nullptr)
    {
        if (existing->peerConnection == nullptr)
        {
            return CHIP_ERROR_INCORRECT_STATE;
        }

        std::vector<uint16_t> newVideoStreams = args.videoStreams.HasValue() ? args.videoStreams.Value() : existing->videoStreams;
        std::vector<uint16_t> newAudioStreams = args.audioStreams.HasValue() ? args.audioStreams.Value() : existing->audioStreams;

        if (newVideoStreams != existing->videoStreams || newAudioStreams != existing->audioStreams)
        {
            ReturnErrorOnFailure(AcquireStreams(newAudioStreams, newVideoStreams));
            if (existing->streamsAcquired)
            {
                ReleaseStreams(existing->audioStreams, existing->videoStreams);
            }
            existing->videoStreams    = std::move(newVideoStreams);
            existing->audioStreams    = std::move(newAudioStreams);
            existing->streamsAcquired = true;
        }

        PopulateOutSession(args, *existing, outSession);
        existing->state                = SessionState::kWaitingLocalAnswer;
        existing->localDescriptionSent = false;
        existing->hasRemoteDescription = true;

        return existing->peerConnection->SetRemoteDescription(webrtc::SdpType::kOffer, CharSpan(args.sdp.data(), args.sdp.size()));
    }

    if (mSessions.size() >= mContext.maxConcurrentSessions)
    {
        ChipLogProgress(Camera, "WebRTCProviderSessionDelegate: max concurrent sessions (%u) reached on ProvideOffer",
                        mContext.maxConcurrentSessions);
        return CHIP_IM_GLOBAL_STATUS(ResourceExhausted);
    }

    ConvertedIceServers iceServers;
    ReturnErrorOnFailure(iceServers.Init(args.iceServers));

    std::optional<webrtc::SFrameConfig> sFrameConfig;
    if (args.sFrameConfig.HasValue())
    {
        sFrameConfig = webrtc::SFrameConfig{
            .cipherSuite = args.sFrameConfig.Value().cipherSuite,
            .baseKey     = args.sFrameConfig.Value().baseKey,
            .kid         = args.sFrameConfig.Value().kid,
        };
    }

    auto session                   = std::make_unique<Session>(*this, args.sessionId);
    session->peer                  = ScopedNodeId(args.peerNodeId, args.fabricIndex);
    session->originatingEndpointId = args.originatingEndpointId;
    session->streamUsage           = args.streamUsage;
    if (args.videoStreams.HasValue())
    {
        session->videoStreams = args.videoStreams.Value();
    }
    if (args.audioStreams.HasValue())
    {
        session->audioStreams = args.audioStreams.Value();
    }

    webrtc::PeerConnectionFactory::SessionConfig config{
        .sessionId          = args.sessionId,
        .iceServers         = Span<const webrtc::IceServer>(iceServers.servers.data(), iceServers.servers.size()),
        .iceTransportPolicy = ParseIceTransportPolicy(args.iceTransportPolicy),
        .sFrameConfig       = sFrameConfig,
        .videoStreamIds     = Span<const uint16_t>(session->videoStreams.data(), session->videoStreams.size()),
        .audioStreamIds     = Span<const uint16_t>(session->audioStreams.data(), session->audioStreams.size()),
    };

    session->peerConnection = mContext.peerFactory.Create(config, session->events);
    VerifyOrReturnError(session->peerConnection != nullptr, CHIP_ERROR_NO_MEMORY);

    CHIP_ERROR err = AcquireStreams(session->audioStreams, session->videoStreams);
    if (err != CHIP_NO_ERROR)
    {
        session->peerConnection->Close();
        session->peerConnection.reset();
        return err;
    }
    session->streamsAcquired = true;

    PopulateOutSession(args, *session, outSession);

    Session * rawSession             = session.get();
    mSessions[args.sessionId]        = std::move(session);
    rawSession->state                = SessionState::kWaitingLocalAnswer;
    rawSession->hasRemoteDescription = true;

    if (mContext.connectionTimeoutSeconds > 0)
    {
        LogErrorOnFailure(mContext.timerDelegate.StartTimer(&rawSession->timeoutTimer,
                                                            System::Clock::Seconds32(mContext.connectionTimeoutSeconds)));
    }

    err = rawSession->peerConnection->SetRemoteDescription(webrtc::SdpType::kOffer, CharSpan(args.sdp.data(), args.sdp.size()));
    if (err != CHIP_NO_ERROR)
    {
        CleanupSession(args.sessionId);
        return err;
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR WebRTCProviderSessionDelegate::HandleProvideAnswer(uint16_t sessionId, const std::string & sdpAnswer)
{
    Session * session = FindSession(sessionId);
    VerifyOrReturnError(session != nullptr, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(session->peerConnection != nullptr, CHIP_ERROR_INCORRECT_STATE);

    if (!ValidateSdpFields(std::string_view(sdpAnswer.data(), sdpAnswer.size())))
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    if (session->hasRemoteDescription)
    {
        ChipLogProgress(Camera, "WebRTCProviderSessionDelegate: ignoring duplicate answer for session %u", sessionId);
        return CHIP_NO_ERROR;
    }

    session->hasRemoteDescription = true;
    session->state                = SessionState::kActive;
    ReturnErrorOnFailure(
        session->peerConnection->SetRemoteDescription(webrtc::SdpType::kAnswer, CharSpan(sdpAnswer.data(), sdpAnswer.size())));

    if (session->localDescriptionSent && !session->pendingLocalCandidates.empty())
    {
        FlushPendingCandidates(*session);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR WebRTCProviderSessionDelegate::HandleProvideICECandidates(uint16_t sessionId,
                                                                     const std::vector<ICECandidateStruct> & candidates)
{
    Session * session = FindSession(sessionId);
    VerifyOrReturnError(session != nullptr, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(session->peerConnection != nullptr, CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(!candidates.empty(), CHIP_ERROR_INVALID_ARGUMENT);

    for (const auto & c : candidates)
    {
        webrtc::IceCandidate ice{
            .candidate     = c.candidate,
            .sdpMid        = c.SDPMid.IsNull() ? std::nullopt : std::make_optional(c.SDPMid.Value()),
            .sdpMLineIndex = c.SDPMLineIndex.IsNull() ? std::nullopt : std::make_optional(c.SDPMLineIndex.Value()),
        };
        ReturnErrorOnFailure(session->peerConnection->AddRemoteCandidate(ice));
    }

    if (session->localDescriptionSent && !session->pendingLocalCandidates.empty())
    {
        FlushPendingCandidates(*session);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR WebRTCProviderSessionDelegate::HandleEndSession(uint16_t sessionId, WebRTCEndReasonEnum reasonCode)
{
    Session * session = FindSession(sessionId);
    VerifyOrReturnError(session != nullptr, CHIP_ERROR_INVALID_ARGUMENT);

    CleanupSession(sessionId);
    return CHIP_NO_ERROR;
}

CHIP_ERROR WebRTCProviderSessionDelegate::ValidateStreamUsage(StreamUsageEnum streamUsage,
                                                              Optional<std::vector<uint16_t>> & videoStreams,
                                                              Optional<std::vector<uint16_t>> & audioStreams)
{
    const auto & supportedUsages = mContext.avsmCluster.GetSupportedStreamUsages();
    if (std::find(supportedUsages.begin(), supportedUsages.end(), streamUsage) == supportedUsages.end())
    {
        ChipLogError(Camera, "WebRTCProviderSessionDelegate: stream usage %u not supported", to_underlying(streamUsage));
        return CHIP_ERROR_NOT_FOUND;
    }

    if (videoStreams.HasValue() && videoStreams.Value().empty())
    {
        const auto & allocated = mContext.avsmCluster.GetAllocatedVideoStreams();
        std::optional<uint16_t> fallbackId;
        bool exactMatch = false;
        for (const auto & stream : allocated)
        {
            if (stream.streamUsage == streamUsage)
            {
                videoStreams.Value().push_back(stream.videoStreamID);
                exactMatch = true;
                break;
            }
            if (!fallbackId.has_value())
            {
                fallbackId = stream.videoStreamID;
            }
        }
        if (!exactMatch && fallbackId.has_value())
        {
            videoStreams.Value().push_back(*fallbackId);
        }
    }

    if (audioStreams.HasValue() && audioStreams.Value().empty())
    {
        const auto & allocated = mContext.avsmCluster.GetAllocatedAudioStreams();
        std::optional<uint16_t> fallbackId;
        bool exactMatch = false;
        for (const auto & stream : allocated)
        {
            if (stream.streamUsage == streamUsage)
            {
                audioStreams.Value().push_back(stream.audioStreamID);
                exactMatch = true;
                break;
            }
            if (!fallbackId.has_value())
            {
                fallbackId = stream.audioStreamID;
            }
        }
        if (!exactMatch && fallbackId.has_value())
        {
            audioStreams.Value().push_back(*fallbackId);
        }
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR WebRTCProviderSessionDelegate::ValidateVideoStreamID(uint16_t videoStreamId)
{
    const auto & allocated = mContext.avsmCluster.GetAllocatedVideoStreams();
    bool found             = std::any_of(allocated.begin(), allocated.end(),
                                         [videoStreamId](const auto & stream) { return stream.videoStreamID == videoStreamId; });
    return found ? CHIP_NO_ERROR : CHIP_ERROR_INVALID_ARGUMENT;
}

CHIP_ERROR WebRTCProviderSessionDelegate::ValidateAudioStreamID(uint16_t audioStreamId)
{
    const auto & allocated = mContext.avsmCluster.GetAllocatedAudioStreams();
    bool found             = std::any_of(allocated.begin(), allocated.end(),
                                         [audioStreamId](const auto & stream) { return stream.audioStreamID == audioStreamId; });
    return found ? CHIP_NO_ERROR : CHIP_ERROR_INVALID_ARGUMENT;
}

CHIP_ERROR WebRTCProviderSessionDelegate::ValidateVideoStreams(const std::vector<uint16_t> & videoStreams)
{
    return ValidateStreamsExist(videoStreams, mContext.avsmCluster.GetAllocatedVideoStreams(),
                                [](const auto & stream) { return stream.videoStreamID; });
}

CHIP_ERROR WebRTCProviderSessionDelegate::ValidateAudioStreams(const std::vector<uint16_t> & audioStreams)
{
    return ValidateStreamsExist(audioStreams, mContext.avsmCluster.GetAllocatedAudioStreams(),
                                [](const auto & stream) { return stream.audioStreamID; });
}

CHIP_ERROR WebRTCProviderSessionDelegate::IsStreamUsageSupported(StreamUsageEnum streamUsage)
{
    const auto & priorities = mContext.avsmCluster.GetStreamUsagePriorities();
    bool found              = std::find(priorities.begin(), priorities.end(), streamUsage) != priorities.end();
    return found ? CHIP_NO_ERROR : CHIP_ERROR_NOT_FOUND;
}

CHIP_ERROR WebRTCProviderSessionDelegate::IsHardPrivacyModeActive(bool & isActive)
{
    isActive = mContext.avsmCluster.GetHardPrivacyModeOn();
    return CHIP_NO_ERROR;
}

CHIP_ERROR WebRTCProviderSessionDelegate::IsSoftRecordingPrivacyModeActive(bool & isActive)
{
    isActive = mContext.avsmCluster.GetSoftRecordingPrivacyModeEnabled();
    return CHIP_NO_ERROR;
}

CHIP_ERROR WebRTCProviderSessionDelegate::IsSoftLivestreamPrivacyModeActive(bool & isActive)
{
    isActive = mContext.avsmCluster.GetSoftLivestreamPrivacyModeEnabled();
    return CHIP_NO_ERROR;
}

bool WebRTCProviderSessionDelegate::HasAllocatedVideoStreams()
{
    return !mContext.avsmCluster.GetAllocatedVideoStreams().empty();
}

bool WebRTCProviderSessionDelegate::HasAllocatedAudioStreams()
{
    return !mContext.avsmCluster.GetAllocatedAudioStreams().empty();
}

CHIP_ERROR WebRTCProviderSessionDelegate::ValidateSFrameConfig(uint16_t cipherSuite, size_t baseKeyLength)
{
    size_t expectedLength = 0;
    switch (cipherSuite)
    {
    case kCipherSuiteAes128Gcm:
        expectedLength = kAes128KeyLength;
        break;
    case kCipherSuiteAes256Gcm:
        expectedLength = kAes256KeyLength;
        break;
    default:
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    return (baseKeyLength == expectedLength) ? CHIP_NO_ERROR : CHIP_ERROR_INVALID_ARGUMENT;
}

CHIP_ERROR WebRTCProviderSessionDelegate::IsUTCTimeNull(bool & isNull)
{
    isNull = mContext.isUtcTimeNull ? mContext.isUtcTimeNull() : false;
    return CHIP_NO_ERROR;
}

void WebRTCProviderSessionDelegate::OnSoftLivestreamPrivacyModeChanged(bool enabled)
{
    if (!enabled)
    {
        return;
    }
    EndSessionsMatching(
        [](const Session & s) { return s.state != SessionState::kPendingEnd && s.streamUsage == StreamUsageEnum::kLiveView; },
        WebRTCEndReasonEnum::kPrivacyMode);
}

void WebRTCProviderSessionDelegate::OnSoftRecordingPrivacyModeChanged(bool enabled)
{
    if (!enabled)
    {
        return;
    }
    EndSessionsMatching(
        [](const Session & s) {
            return s.state != SessionState::kPendingEnd &&
                (s.streamUsage == StreamUsageEnum::kRecording || s.streamUsage == StreamUsageEnum::kAnalysis);
        },
        WebRTCEndReasonEnum::kPrivacyMode);
}

void WebRTCProviderSessionDelegate::OnHardPrivacyModeChanged(bool enabled)
{
    if (!enabled)
    {
        return;
    }
    EndSessionsMatching([](const Session & s) { return s.state != SessionState::kPendingEnd; }, WebRTCEndReasonEnum::kPrivacyMode);
}

void WebRTCProviderSessionDelegate::OnCommandResult(uint16_t sessionId, CHIP_ERROR err, Protocols::InteractionModel::Status status)
{
    if (err != CHIP_NO_ERROR || status != Protocols::InteractionModel::Status::Success)
    {
        ChipLogError(Camera,
                     "WebRTCProviderSessionDelegate: outbound command for session %u failed: err=%" CHIP_ERROR_FORMAT
                     " status=0x%02x",
                     sessionId, err.Format(), to_underlying(status));
    }
}

void WebRTCProviderSessionDelegate::OnPeerLocalDescription(uint16_t sessionId, webrtc::SdpType type, CharSpan sdp)
{
    Session * session = FindSession(sessionId);
    VerifyOrReturn(session != nullptr);

    if (session->state == SessionState::kWaitingLocalOffer && type == webrtc::SdpType::kOffer)
    {
        session->state                = SessionState::kWaitingRemoteAnswer;
        session->localDescriptionSent = true;
        CHIP_ERROR err = mContext.requestorClient.SendOffer(session->peer, session->originatingEndpointId, sessionId, sdp, this);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(Camera, "WebRTCProviderSessionDelegate: SendOffer(%u) failed: %" CHIP_ERROR_FORMAT, sessionId,
                         err.Format());
            return;
        }
        FlushPendingCandidates(*session);
    }
    else if (session->state == SessionState::kWaitingLocalAnswer && type == webrtc::SdpType::kAnswer)
    {
        session->state                = SessionState::kActive;
        session->localDescriptionSent = true;
        CHIP_ERROR err = mContext.requestorClient.SendAnswer(session->peer, session->originatingEndpointId, sessionId, sdp, this);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(Camera, "WebRTCProviderSessionDelegate: SendAnswer(%u) failed: %" CHIP_ERROR_FORMAT, sessionId,
                         err.Format());
            return;
        }
        FlushPendingCandidates(*session);
    }
}

void WebRTCProviderSessionDelegate::OnPeerLocalCandidate(uint16_t sessionId, const webrtc::IceCandidate & candidate)
{
    Session * session = FindSession(sessionId);
    VerifyOrReturn(session != nullptr);

    if (!session->localDescriptionSent)
    {
        BufferedCandidate & buffered = session->pendingLocalCandidates.emplace_back();
        buffered.candidate.assign(candidate.candidate.data(), candidate.candidate.size());
        if (candidate.sdpMid.has_value() && !candidate.sdpMid->empty())
        {
            buffered.sdpMid.emplace(candidate.sdpMid->data(), candidate.sdpMid->size());
        }
        buffered.sdpMLineIndex = candidate.sdpMLineIndex;
        return;
    }

    SendSingleCandidate(*session, candidate);
}

void WebRTCProviderSessionDelegate::OnPeerConnected(uint16_t sessionId)
{
    Session * session = FindSession(sessionId);
    VerifyOrReturn(session != nullptr);

    mContext.timerDelegate.CancelTimer(&session->timeoutTimer);
}

void WebRTCProviderSessionDelegate::OnPeerClosed(uint16_t sessionId)
{
    VerifyOrReturn(FindSession(sessionId) != nullptr);
    CleanupSession(sessionId);
}

void WebRTCProviderSessionDelegate::OnSessionTimeout(uint16_t sessionId)
{
    VerifyOrReturn(FindSession(sessionId) != nullptr);
    ChipLogError(Camera, "WebRTCProviderSessionDelegate: connection timeout for session %u", sessionId);
    CleanupSession(sessionId);
}

void WebRTCProviderSessionDelegate::OnPendingEndTimerFired(uint16_t sessionId)
{
    Session * session = FindSession(sessionId);
    VerifyOrReturn(session != nullptr);

    ScopedNodeId peer          = session->peer;
    EndpointId endpoint        = session->originatingEndpointId;
    WebRTCEndReasonEnum reason = session->pendingEndReason;

    CleanupSession(sessionId);
    LogErrorOnFailure(mContext.requestorClient.SendEnd(peer, endpoint, sessionId, reason, this));
}

void WebRTCProviderSessionDelegate::FlushPendingCandidates(Session & session)
{
    if (session.pendingLocalCandidates.empty())
    {
        return;
    }

    std::vector<ICECandidateStruct> wireCandidates;
    wireCandidates.reserve(session.pendingLocalCandidates.size());
    for (const auto & buffered : session.pendingLocalCandidates)
    {
        ICECandidateStruct & c = wireCandidates.emplace_back();
        c.candidate            = CharSpan(buffered.candidate.data(), buffered.candidate.size());
        if (buffered.sdpMid.has_value())
        {
            c.SDPMid.SetNonNull(CharSpan(buffered.sdpMid->data(), buffered.sdpMid->size()));
        }
        else
        {
            c.SDPMid.SetNull();
        }
        if (buffered.sdpMLineIndex.has_value())
        {
            c.SDPMLineIndex.SetNonNull(*buffered.sdpMLineIndex);
        }
        else
        {
            c.SDPMLineIndex.SetNull();
        }
    }

    LogErrorOnFailure(mContext.requestorClient.SendICECandidates(
        session.peer, session.originatingEndpointId, session.sessionId,
        Span<const ICECandidateStruct>(wireCandidates.data(), wireCandidates.size()), this));
    session.pendingLocalCandidates.clear();
}

void WebRTCProviderSessionDelegate::SendSingleCandidate(const Session & session, const webrtc::IceCandidate & candidate)
{
    ICECandidateStruct wireCandidate;
    wireCandidate.candidate = candidate.candidate;
    if (candidate.sdpMid.has_value() && !candidate.sdpMid->empty())
    {
        wireCandidate.SDPMid.SetNonNull(*candidate.sdpMid);
    }
    else
    {
        wireCandidate.SDPMid.SetNull();
    }
    if (candidate.sdpMLineIndex.has_value())
    {
        wireCandidate.SDPMLineIndex.SetNonNull(*candidate.sdpMLineIndex);
    }
    else
    {
        wireCandidate.SDPMLineIndex.SetNull();
    }

    LogErrorOnFailure(mContext.requestorClient.SendICECandidates(session.peer, session.originatingEndpointId, session.sessionId,
                                                                 Span<const ICECandidateStruct>(&wireCandidate, 1), this));
}

void WebRTCProviderSessionDelegate::EndSessionsMatching(const std::function<bool(const Session &)> & predicate,
                                                        WebRTCEndReasonEnum reason)
{
    struct Target
    {
        uint16_t sessionId;
        ScopedNodeId peer;
        EndpointId endpoint;
    };

    std::vector<Target> targets;
    for (const auto & [id, session] : mSessions)
    {
        if (predicate(*session))
        {
            targets.push_back({ id, session->peer, session->originatingEndpointId });
        }
    }

    for (const auto & target : targets)
    {
        CleanupSession(target.sessionId);
        LogErrorOnFailure(mContext.requestorClient.SendEnd(target.peer, target.endpoint, target.sessionId, reason, this));
    }
}

void WebRTCProviderSessionDelegate::CleanupSession(uint16_t sessionId)
{
    auto it = mSessions.find(sessionId);
    if (it == mSessions.end())
    {
        return;
    }

    std::unique_ptr<Session> session = std::move(it->second);
    mSessions.erase(it);

    mContext.timerDelegate.CancelTimer(&session->timeoutTimer);
    mContext.timerDelegate.CancelTimer(&session->pendingEndTimer);
    mContext.requestorClient.CancelForSession(sessionId);

    if (session->peerConnection != nullptr)
    {
        session->peerConnection->Close();
        session->peerConnection.reset();
    }

    if (session->streamsAcquired)
    {
        ReleaseStreams(session->audioStreams, session->videoStreams);
        session->streamsAcquired = false;
    }

    if (mProviderCluster != nullptr)
    {
        mProviderCluster->RemoveSession(sessionId);
    }
}

CHIP_ERROR WebRTCProviderSessionDelegate::AcquireStreams(const std::vector<uint16_t> & audioStreams,
                                                         const std::vector<uint16_t> & videoStreams)
{
    std::vector<uint16_t> acquiredAudio;
    std::vector<uint16_t> acquiredVideo;

    auto rollback = [&]() {
        for (uint16_t id : acquiredAudio)
        {
            LogErrorOnFailure(mContext.avsmCluster.UpdateAudioStreamRefCount(id, /* shouldIncrement = */ false));
        }
        for (uint16_t id : acquiredVideo)
        {
            LogErrorOnFailure(mContext.avsmCluster.UpdateVideoStreamRefCount(id, /* shouldIncrement = */ false));
        }
    };

    for (uint16_t id : audioStreams)
    {
        CHIP_ERROR err = mContext.avsmCluster.UpdateAudioStreamRefCount(id, /* shouldIncrement = */ true);
        if (err != CHIP_NO_ERROR)
        {
            rollback();
            return err;
        }
        acquiredAudio.push_back(id);
    }

    for (uint16_t id : videoStreams)
    {
        CHIP_ERROR err = mContext.avsmCluster.UpdateVideoStreamRefCount(id, /* shouldIncrement = */ true);
        if (err != CHIP_NO_ERROR)
        {
            rollback();
            return err;
        }
        acquiredVideo.push_back(id);
    }

    return CHIP_NO_ERROR;
}

void WebRTCProviderSessionDelegate::ReleaseStreams(const std::vector<uint16_t> & audioStreams,
                                                   const std::vector<uint16_t> & videoStreams)
{
    for (uint16_t id : audioStreams)
    {
        LogErrorOnFailure(mContext.avsmCluster.UpdateAudioStreamRefCount(id, /* shouldIncrement = */ false));
    }

    for (uint16_t id : videoStreams)
    {
        LogErrorOnFailure(mContext.avsmCluster.UpdateVideoStreamRefCount(id, /* shouldIncrement = */ false));
    }
}

} // namespace WebRTCTransportProvider
} // namespace Clusters
} // namespace app
} // namespace chip
