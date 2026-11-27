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

#include <app/CASESessionManager.h>
#include <app/CommandSender.h>
#include <app/data-model/EncodableToTLV.h>
#include <clusters/WebRTCTransportRequestor/Commands.h>
#include <clusters/WebRTCTransportRequestor/Enums.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/ScopedNodeId.h>
#include <lib/support/Span.h>
#include <protocols/interaction_model/StatusCode.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace chip {
namespace app {
namespace webrtc {

/**
 * Sends outbound Matter commands to a peer's WebRTCTransportRequestor cluster over CASE
 * sessions with TransportPayloadCapability::kLargePayload.
 *
 * Each command owns its own CASE connection callbacks and CommandSender, supporting
 * concurrent sessions and pipelined Offer/Answer/ICECandidates/End commands.
 */
class WebRTCTransportRequestorClient
{
public:
    class Callback
    {
    public:
        virtual ~Callback()                                                                                          = default;
        virtual void OnCommandResult(uint16_t sessionId, CHIP_ERROR err, Protocols::InteractionModel::Status status) = 0;
    };

    explicit WebRTCTransportRequestorClient(CASESessionManager & caseSessionManager);
    virtual ~WebRTCTransportRequestorClient();

    WebRTCTransportRequestorClient(const WebRTCTransportRequestorClient &)             = delete;
    WebRTCTransportRequestorClient & operator=(const WebRTCTransportRequestorClient &) = delete;

    CHIP_ERROR SendOffer(const ScopedNodeId & peer, EndpointId endpoint, uint16_t sessionId, CharSpan sdp, Callback * cb = nullptr);
    CHIP_ERROR SendAnswer(const ScopedNodeId & peer, EndpointId endpoint, uint16_t sessionId, CharSpan sdp,
                          Callback * cb = nullptr);
    CHIP_ERROR SendICECandidates(const ScopedNodeId & peer, EndpointId endpoint, uint16_t sessionId,
                                 Span<const Clusters::Globals::Structs::ICECandidateStruct::Type> candidates,
                                 Callback * cb = nullptr);
    CHIP_ERROR SendEnd(const ScopedNodeId & peer, EndpointId endpoint, uint16_t sessionId,
                       Clusters::Globals::WebRTCEndReasonEnum reason, Callback * cb = nullptr);

    void CancelForSession(uint16_t sessionId);
    void CancelAll();

    size_t PendingRequestCount() const { return mPendingRequests.size(); }

protected:
    enum class CommandType : uint8_t
    {
        kOffer,
        kAnswer,
        kICECandidates,
        kEnd,
    };

    struct BufferedICECandidate
    {
        std::string candidate;
        std::optional<std::string> sdpMid;
        DataModel::Nullable<uint16_t> sdpMLineIndex;
    };

    struct PendingRequest : public CommandSender::ExtendableCallback
    {
        PendingRequest(WebRTCTransportRequestorClient & clientRef, CommandType type, const ScopedNodeId & peerNode,
                       EndpointId targetEndpoint, uint16_t session, Callback * cb);
        ~PendingRequest() override;

        void OnResponse(CommandSender * commandSender, const CommandSender::ResponseData & responseData) override;
        void OnError(const CommandSender * commandSender, const CommandSender::ErrorData & errorData) override;
        void OnDone(CommandSender * commandSender) override;

        WebRTCTransportRequestorClient & client;
        CommandType commandType;
        ScopedNodeId peer;
        EndpointId endpoint = kInvalidEndpointId;
        uint16_t sessionId  = 0;
        Callback * callback = nullptr;

        std::string sdp;
        std::vector<BufferedICECandidate> candidates;
        Clusters::Globals::WebRTCEndReasonEnum endReason = Clusters::Globals::WebRTCEndReasonEnum::kUnknownEnumValue;

        chip::Callback::Callback<OnDeviceConnected> onConnected;
        chip::Callback::Callback<OnDeviceConnectionFailure> onConnectionFailure;
        Platform::UniquePtr<CommandSender> commandSender;

        CHIP_ERROR resultError                           = CHIP_NO_ERROR;
        Protocols::InteractionModel::Status resultStatus = Protocols::InteractionModel::Status::Success;
        bool hasResult                                   = false;
        bool cancelled                                   = false;
    };

    virtual void EstablishSession(PendingRequest & request);
    virtual CHIP_ERROR SendCommand(PendingRequest & request, Messaging::ExchangeManager & exchangeMgr,
                                   const SessionHandle & sessionHandle);

    CHIP_ERROR BuildOffer(const PendingRequest & request, uint16_t & outWireSessionId,
                          Clusters::WebRTCTransportRequestor::Commands::Offer::Type & outCmd) const;
    CHIP_ERROR BuildAnswer(const PendingRequest & request, uint16_t & outWireSessionId,
                           Clusters::WebRTCTransportRequestor::Commands::Answer::Type & outCmd) const;
    CHIP_ERROR BuildICECandidates(const PendingRequest & request, uint16_t & outWireSessionId,
                                  std::vector<Clusters::Globals::Structs::ICECandidateStruct::Type> & outCandidates,
                                  Clusters::WebRTCTransportRequestor::Commands::ICECandidates::Type & outCmd) const;
    CHIP_ERROR BuildEnd(const PendingRequest & request, Clusters::WebRTCTransportRequestor::Commands::End::Type & outCmd) const;

    CHIP_ERROR InvokeOnSession(PendingRequest & request, CommandId commandId, const DataModel::EncodableToTLV & payload,
                               Messaging::ExchangeManager & exchangeMgr, const SessionHandle & sessionHandle);

    std::unique_ptr<PendingRequest> TakeRequest(PendingRequest * request);
    const std::vector<std::unique_ptr<PendingRequest>> & PendingRequests() const { return mPendingRequests; }

private:
    static void OnDeviceConnectedCallback(void * context, Messaging::ExchangeManager & exchangeMgr,
                                          const SessionHandle & sessionHandle);
    static void OnDeviceConnectionFailureCallback(void * context, const ScopedNodeId & peerId, CHIP_ERROR error);

    CHIP_ERROR EnqueueAndStart(std::unique_ptr<PendingRequest> request);
    void CancelRequest(PendingRequest & request);

    CASESessionManager & mCASESessionManager;
    std::vector<std::unique_ptr<PendingRequest>> mPendingRequests;
};

} // namespace webrtc
} // namespace app
} // namespace chip
