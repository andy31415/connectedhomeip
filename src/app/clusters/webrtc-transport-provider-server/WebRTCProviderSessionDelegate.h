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
#include <app/clusters/camera-av-stream-management-server/StreamAllocator.h>
#include <app/clusters/webrtc-transport-provider-server/WebRTCTransportProviderCluster.h>
#include <app/webrtc/PeerConnection.h>
#include <app/webrtc/WebRTCTransportRequestorClient.h>
#include <lib/core/CHIPError.h>
#include <lib/core/ScopedNodeId.h>
#include <lib/support/TimerDelegate.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace chip {
namespace app {
namespace Clusters {
namespace WebRTCTransportProvider {

/**
 * SDK implementation of WebRTCTransportProvider::Delegate owning session state,
 * connection timeout timers, SFrame validation, AVSM stream validation/ref-counting,
 * privacy-mode enforcement, and outbound WebRTCTransportRequestor signaling.
 */
class WebRTCProviderSessionDelegate : public Delegate,
                                      public CameraAvStreamManagement::StreamAllocator::PrivacyObserver,
                                      public webrtc::WebRTCTransportRequestorClient::Callback
{
public:
    struct Context
    {
        webrtc::PeerConnectionFactory & peerFactory;
        webrtc::WebRTCTransportRequestorClient & requestorClient;
        CameraAvStreamManagement::CameraAVStreamManagementCluster & avsmCluster;
        TimerDelegate & timerDelegate;
        std::function<bool()> isUtcTimeNull = nullptr;
        uint8_t maxConcurrentSessions       = 5;
        uint32_t connectionTimeoutSeconds   = 30;
        bool deferredOffer                  = false;
    };

    explicit WebRTCProviderSessionDelegate(Context context);
    ~WebRTCProviderSessionDelegate() override;

    WebRTCProviderSessionDelegate(const WebRTCProviderSessionDelegate &)             = delete;
    WebRTCProviderSessionDelegate & operator=(const WebRTCProviderSessionDelegate &) = delete;

    void SetWebRTCTransportProvider(WebRTCTransportProviderCluster * providerCluster) { mProviderCluster = providerCluster; }
    void Shutdown();

    size_t SessionCount() const { return mSessions.size(); }
    bool HasSession(uint16_t sessionId) const { return mSessions.find(sessionId) != mSessions.end(); }

    // WebRTCTransportProvider::Delegate
    CHIP_ERROR HandleSolicitOffer(const OfferRequestArgs & args, WebRTCSessionStruct & outSession,
                                  bool & outDeferredOffer) override;
    CHIP_ERROR HandleProvideOffer(const ProvideOfferRequestArgs & args, WebRTCSessionStruct & outSession) override;
    CHIP_ERROR HandleProvideAnswer(uint16_t sessionId, const std::string & sdpAnswer) override;
    CHIP_ERROR HandleProvideICECandidates(uint16_t sessionId, const std::vector<ICECandidateStruct> & candidates) override;
    CHIP_ERROR HandleEndSession(uint16_t sessionId, WebRTCEndReasonEnum reasonCode) override;

    CHIP_ERROR ValidateStreamUsage(StreamUsageEnum streamUsage, Optional<std::vector<uint16_t>> & videoStreams,
                                   Optional<std::vector<uint16_t>> & audioStreams) override;
    CHIP_ERROR ValidateVideoStreamID(uint16_t videoStreamId) override;
    CHIP_ERROR ValidateAudioStreamID(uint16_t audioStreamId) override;
    CHIP_ERROR ValidateVideoStreams(const std::vector<uint16_t> & videoStreams) override;
    CHIP_ERROR ValidateAudioStreams(const std::vector<uint16_t> & audioStreams) override;
    CHIP_ERROR IsStreamUsageSupported(StreamUsageEnum streamUsage) override;
    CHIP_ERROR IsHardPrivacyModeActive(bool & isActive) override;
    CHIP_ERROR IsSoftRecordingPrivacyModeActive(bool & isActive) override;
    CHIP_ERROR IsSoftLivestreamPrivacyModeActive(bool & isActive) override;
    bool HasAllocatedVideoStreams() override;
    bool HasAllocatedAudioStreams() override;
    CHIP_ERROR ValidateSFrameConfig(uint16_t cipherSuite, size_t baseKeyLength) override;
    CHIP_ERROR IsUTCTimeNull(bool & isNull) override;

    // CameraAvStreamManagement::StreamAllocator::PrivacyObserver
    void OnSoftLivestreamPrivacyModeChanged(bool enabled) override;
    void OnSoftRecordingPrivacyModeChanged(bool enabled) override;
    void OnHardPrivacyModeChanged(bool enabled) override;

    // webrtc::WebRTCTransportRequestorClient::Callback
    void OnCommandResult(uint16_t sessionId, CHIP_ERROR err, Protocols::InteractionModel::Status status) override;

private:
    enum class SessionState : uint8_t
    {
        kWaitingLocalOffer,
        kWaitingRemoteAnswer,
        kWaitingLocalAnswer,
        kActive,
        kPendingEnd,
    };

    struct BufferedCandidate
    {
        std::string candidate;
        std::optional<std::string> sdpMid;
        std::optional<uint16_t> sdpMLineIndex;
    };

    struct Session
    {
        class EventsAdapter : public webrtc::PeerConnection::Events
        {
        public:
            EventsAdapter(WebRTCProviderSessionDelegate & ownerRef, uint16_t id) : owner(ownerRef), sessionId(id) {}
            void OnLocalDescription(webrtc::SdpType type, CharSpan sdp) override
            {
                owner.OnPeerLocalDescription(sessionId, type, sdp);
            }
            void OnLocalCandidate(const webrtc::IceCandidate & candidate) override
            {
                owner.OnPeerLocalCandidate(sessionId, candidate);
            }
            void OnConnected() override { owner.OnPeerConnected(sessionId); }
            void OnClosed() override { owner.OnPeerClosed(sessionId); }

            WebRTCProviderSessionDelegate & owner;
            uint16_t sessionId;
        };

        class TimeoutTimer : public TimerContext
        {
        public:
            TimeoutTimer(WebRTCProviderSessionDelegate & ownerRef, uint16_t id) : owner(ownerRef), sessionId(id) {}
            void TimerFired() override { owner.OnSessionTimeout(sessionId); }

            WebRTCProviderSessionDelegate & owner;
            uint16_t sessionId;
        };

        class PendingEndTimer : public TimerContext
        {
        public:
            PendingEndTimer(WebRTCProviderSessionDelegate & ownerRef, uint16_t id) : owner(ownerRef), sessionId(id) {}
            void TimerFired() override { owner.OnPendingEndTimerFired(sessionId); }

            WebRTCProviderSessionDelegate & owner;
            uint16_t sessionId;
        };

        Session(WebRTCProviderSessionDelegate & owner, uint16_t id) :
            sessionId(id), events(owner, id), timeoutTimer(owner, id), pendingEndTimer(owner, id)
        {}

        uint16_t sessionId = 0;
        ScopedNodeId peer;
        EndpointId originatingEndpointId = kInvalidEndpointId;
        StreamUsageEnum streamUsage      = StreamUsageEnum::kUnknownEnumValue;
        std::vector<uint16_t> videoStreams;
        std::vector<uint16_t> audioStreams;
        SessionState state = SessionState::kWaitingLocalOffer;

        bool localDescriptionSent            = false;
        bool hasRemoteDescription            = false;
        bool streamsAcquired                 = false;
        WebRTCEndReasonEnum pendingEndReason = WebRTCEndReasonEnum::kOutOfResources;

        std::vector<BufferedCandidate> pendingLocalCandidates;
        std::unique_ptr<webrtc::PeerConnection> peerConnection;

        EventsAdapter events;
        TimeoutTimer timeoutTimer;
        PendingEndTimer pendingEndTimer;
    };

    void OnPeerLocalDescription(uint16_t sessionId, webrtc::SdpType type, CharSpan sdp);
    void OnPeerLocalCandidate(uint16_t sessionId, const webrtc::IceCandidate & candidate);
    void OnPeerConnected(uint16_t sessionId);
    void OnPeerClosed(uint16_t sessionId);
    void OnSessionTimeout(uint16_t sessionId);
    void OnPendingEndTimerFired(uint16_t sessionId);

    void FlushPendingCandidates(Session & session);
    void SendSingleCandidate(const Session & session, const webrtc::IceCandidate & candidate);
    void CleanupSession(uint16_t sessionId);
    void EndSessionsMatching(const std::function<bool(const Session &)> & predicate, WebRTCEndReasonEnum reason);

    CHIP_ERROR AcquireStreams(const std::vector<uint16_t> & audioStreams, const std::vector<uint16_t> & videoStreams);
    void ReleaseStreams(const std::vector<uint16_t> & audioStreams, const std::vector<uint16_t> & videoStreams);

    Session * FindSession(uint16_t sessionId);
    static void PopulateOutSession(const OfferRequestArgs & args, const Session & session, WebRTCSessionStruct & outSession);

    Context mContext;
    WebRTCTransportProviderCluster * mProviderCluster = nullptr;
    std::unordered_map<uint16_t, std::unique_ptr<Session>> mSessions;
};

} // namespace WebRTCTransportProvider
} // namespace Clusters
} // namespace app
} // namespace chip
