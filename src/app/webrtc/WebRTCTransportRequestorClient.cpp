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

#include <app/webrtc/WebRTCTransportRequestorClient.h>

#include <app/CommandPathParams.h>
#include <app/MessageDef/StatusIB.h>
#include <clusters/WebRTCTransportRequestor/Ids.h>
#include <lib/support/CHIPFaultInjection.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

#include <algorithm>

using chip::Protocols::InteractionModel::Status;

namespace chip {
namespace app {
namespace webrtc {

WebRTCTransportRequestorClient::PendingRequest::PendingRequest(WebRTCTransportRequestorClient & clientRef, CommandType type,
                                                               const ScopedNodeId & peerNode, EndpointId targetEndpoint,
                                                               uint16_t session, Callback * cb) :
    client(clientRef), commandType(type), peer(peerNode), endpoint(targetEndpoint), sessionId(session), callback(cb),
    onConnected(OnDeviceConnectedCallback, this), onConnectionFailure(OnDeviceConnectionFailureCallback, this)
{}

WebRTCTransportRequestorClient::PendingRequest::~PendingRequest()
{
    onConnected.Cancel();
    onConnectionFailure.Cancel();
}

void WebRTCTransportRequestorClient::PendingRequest::OnResponse(CommandSender * sender,
                                                                const CommandSender::ResponseData & responseData)
{
    if (cancelled || commandSender.get() != sender)
    {
        return;
    }

    hasResult    = true;
    resultStatus = responseData.statusIB.mStatus;
    resultError  = responseData.statusIB.IsSuccess() ? CHIP_NO_ERROR : responseData.statusIB.ToChipError();
}

void WebRTCTransportRequestorClient::PendingRequest::OnError(const CommandSender * sender,
                                                             const CommandSender::ErrorData & errorData)
{
    if (cancelled || commandSender.get() != sender)
    {
        return;
    }

    hasResult    = true;
    resultError  = errorData.error;
    resultStatus = errorData.error.IsIMStatus() ? StatusIB(errorData.error).mStatus : Protocols::InteractionModel::Status::Failure;
}

void WebRTCTransportRequestorClient::PendingRequest::OnDone(CommandSender * sender)
{
    if (commandSender.get() != sender)
    {
        return;
    }

    // Hold self alive on the stack while removing from mPendingRequests and resetting CommandSender.
    std::unique_ptr<PendingRequest> self = client.TakeRequest(this);
    commandSender.reset();

    if (cancelled)
    {
        return;
    }

    if (!hasResult)
    {
        resultError  = CHIP_ERROR_INCORRECT_STATE;
        resultStatus = Status::Failure;
    }

    if (resultError != CHIP_NO_ERROR)
    {
        ChipLogError(Zcl, "WebRTCTransportRequestorClient: command %u for session %u failed: %" CHIP_ERROR_FORMAT,
                     static_cast<unsigned>(commandType), sessionId, resultError.Format());
    }

    if (callback != nullptr)
    {
        Callback * cb = callback;
        callback      = nullptr;
        cb->OnCommandResult(sessionId, resultError, resultStatus);
    }
}

WebRTCTransportRequestorClient::WebRTCTransportRequestorClient(CASESessionManager & caseSessionManager) :
    mCASESessionManager(caseSessionManager)
{}

WebRTCTransportRequestorClient::~WebRTCTransportRequestorClient()
{
    CancelAll();
}

CHIP_ERROR WebRTCTransportRequestorClient::SendOffer(const ScopedNodeId & peer, EndpointId endpoint, uint16_t sessionId,
                                                     CharSpan sdp, Callback * cb)
{
    VerifyOrReturnError(peer.IsOperational(), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(endpoint != kInvalidEndpointId, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(!sdp.empty(), CHIP_ERROR_INVALID_ARGUMENT);

    auto request = std::make_unique<PendingRequest>(*this, CommandType::kOffer, peer, endpoint, sessionId, cb);
    request->sdp.assign(sdp.data(), sdp.size());
    return EnqueueAndStart(std::move(request));
}

CHIP_ERROR WebRTCTransportRequestorClient::SendAnswer(const ScopedNodeId & peer, EndpointId endpoint, uint16_t sessionId,
                                                      CharSpan sdp, Callback * cb)
{
    VerifyOrReturnError(peer.IsOperational(), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(endpoint != kInvalidEndpointId, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(!sdp.empty(), CHIP_ERROR_INVALID_ARGUMENT);

    auto request = std::make_unique<PendingRequest>(*this, CommandType::kAnswer, peer, endpoint, sessionId, cb);
    request->sdp.assign(sdp.data(), sdp.size());
    return EnqueueAndStart(std::move(request));
}

CHIP_ERROR
WebRTCTransportRequestorClient::SendICECandidates(const ScopedNodeId & peer, EndpointId endpoint, uint16_t sessionId,
                                                  Span<const Clusters::Globals::Structs::ICECandidateStruct::Type> candidates,
                                                  Callback * cb)
{
    VerifyOrReturnError(peer.IsOperational(), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(endpoint != kInvalidEndpointId, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(!candidates.empty(), CHIP_ERROR_INVALID_ARGUMENT);

    auto request = std::make_unique<PendingRequest>(*this, CommandType::kICECandidates, peer, endpoint, sessionId, cb);
    request->candidates.reserve(candidates.size());
    for (const auto & c : candidates)
    {
        BufferedICECandidate & buffered = request->candidates.emplace_back();
        buffered.candidate.assign(c.candidate.data(), c.candidate.size());
        if (!c.SDPMid.IsNull() && !c.SDPMid.Value().empty())
        {
            buffered.sdpMid.emplace(c.SDPMid.Value().data(), c.SDPMid.Value().size());
        }
        buffered.sdpMLineIndex = c.SDPMLineIndex;
    }
    return EnqueueAndStart(std::move(request));
}

CHIP_ERROR WebRTCTransportRequestorClient::SendEnd(const ScopedNodeId & peer, EndpointId endpoint, uint16_t sessionId,
                                                   Clusters::Globals::WebRTCEndReasonEnum reason, Callback * cb)
{
    VerifyOrReturnError(peer.IsOperational(), CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(endpoint != kInvalidEndpointId, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(reason != Clusters::Globals::WebRTCEndReasonEnum::kUnknownEnumValue, CHIP_ERROR_INVALID_ARGUMENT);

    auto request       = std::make_unique<PendingRequest>(*this, CommandType::kEnd, peer, endpoint, sessionId, cb);
    request->endReason = reason;
    return EnqueueAndStart(std::move(request));
}

void WebRTCTransportRequestorClient::CancelForSession(uint16_t sessionId)
{
    std::vector<std::unique_ptr<PendingRequest>> toCancel;
    auto it = mPendingRequests.begin();
    while (it != mPendingRequests.end())
    {
        if ((*it)->sessionId == sessionId)
        {
            toCancel.push_back(std::move(*it));
            it = mPendingRequests.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto & req : toCancel)
    {
        CancelRequest(*req);
    }
}

void WebRTCTransportRequestorClient::CancelAll()
{
    std::vector<std::unique_ptr<PendingRequest>> toCancel;
    toCancel.swap(mPendingRequests);

    for (auto & req : toCancel)
    {
        CancelRequest(*req);
    }
}

void WebRTCTransportRequestorClient::CancelRequest(PendingRequest & request)
{
    request.cancelled = true;
    request.callback  = nullptr;
    request.onConnected.Cancel();
    request.onConnectionFailure.Cancel();
    request.commandSender.reset();
}

CHIP_ERROR WebRTCTransportRequestorClient::EnqueueAndStart(std::unique_ptr<PendingRequest> request)
{
    PendingRequest * rawPtr = request.get();
    mPendingRequests.push_back(std::move(request));
    EstablishSession(*rawPtr);
    return CHIP_NO_ERROR;
}

std::unique_ptr<WebRTCTransportRequestorClient::PendingRequest>
WebRTCTransportRequestorClient::TakeRequest(PendingRequest * request)
{
    for (auto it = mPendingRequests.begin(); it != mPendingRequests.end(); ++it)
    {
        if (it->get() == request)
        {
            std::unique_ptr<PendingRequest> owned = std::move(*it);
            mPendingRequests.erase(it);
            return owned;
        }
    }
    return nullptr;
}

void WebRTCTransportRequestorClient::EstablishSession(PendingRequest & request)
{
    mCASESessionManager.FindOrEstablishSession(request.peer, &request.onConnected, &request.onConnectionFailure,
                                               TransportPayloadCapability::kLargePayload);
}

void WebRTCTransportRequestorClient::OnDeviceConnectedCallback(void * context, Messaging::ExchangeManager & exchangeMgr,
                                                               const SessionHandle & sessionHandle)
{
    auto * request = static_cast<PendingRequest *>(context);
    VerifyOrReturn(request != nullptr && !request->cancelled);

    CHIP_ERROR err = request->client.SendCommand(*request, exchangeMgr, sessionHandle);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Zcl, "WebRTCTransportRequestorClient: SendCommand failed for session %u: %" CHIP_ERROR_FORMAT,
                     request->sessionId, err.Format());
        std::unique_ptr<PendingRequest> owned = request->client.TakeRequest(request);
        if (owned && !owned->cancelled && owned->callback != nullptr)
        {
            Callback * cb   = owned->callback;
            owned->callback = nullptr;
            cb->OnCommandResult(owned->sessionId, err, Status::Failure);
        }
    }
}

void WebRTCTransportRequestorClient::OnDeviceConnectionFailureCallback(void * context, const ScopedNodeId & peerId,
                                                                       CHIP_ERROR error)
{
    auto * request = static_cast<PendingRequest *>(context);
    VerifyOrReturn(request != nullptr && !request->cancelled);

    ChipLogError(Zcl, "WebRTCTransportRequestorClient: CASE connection failed for session %u: %" CHIP_ERROR_FORMAT,
                 request->sessionId, error.Format());

    std::unique_ptr<PendingRequest> owned = request->client.TakeRequest(request);
    if (owned && !owned->cancelled && owned->callback != nullptr)
    {
        Callback * cb   = owned->callback;
        owned->callback = nullptr;
        cb->OnCommandResult(owned->sessionId, error, Status::Failure);
    }
}

CHIP_ERROR WebRTCTransportRequestorClient::BuildOffer(const PendingRequest & request, uint16_t & outWireSessionId,
                                                      Clusters::WebRTCTransportRequestor::Commands::Offer::Type & outCmd) const
{
    outWireSessionId = request.sessionId;
    CHIP_FAULT_INJECT(chip::FaultInjection::kFault_ModifyWebRTCOfferSessionId, outWireSessionId++);

    outCmd.webRTCSessionID = outWireSessionId;
    outCmd.sdp             = CharSpan(request.sdp.data(), request.sdp.size());
    return CHIP_NO_ERROR;
}

CHIP_ERROR WebRTCTransportRequestorClient::BuildAnswer(const PendingRequest & request, uint16_t & outWireSessionId,
                                                       Clusters::WebRTCTransportRequestor::Commands::Answer::Type & outCmd) const
{
    outWireSessionId = request.sessionId;
    CHIP_FAULT_INJECT(chip::FaultInjection::kFault_ModifyWebRTCAnswerSessionId, outWireSessionId++);

    outCmd.webRTCSessionID = outWireSessionId;
    outCmd.sdp             = CharSpan(request.sdp.data(), request.sdp.size());
    return CHIP_NO_ERROR;
}

CHIP_ERROR WebRTCTransportRequestorClient::BuildICECandidates(
    const PendingRequest & request, uint16_t & outWireSessionId,
    std::vector<Clusters::Globals::Structs::ICECandidateStruct::Type> & outCandidates,
    Clusters::WebRTCTransportRequestor::Commands::ICECandidates::Type & outCmd) const
{
    VerifyOrReturnError(!request.candidates.empty(), CHIP_ERROR_INCORRECT_STATE);

    outCandidates.clear();
    outCandidates.reserve(request.candidates.size());
    for (const BufferedICECandidate & buffered : request.candidates)
    {
        Clusters::Globals::Structs::ICECandidateStruct::Type & candidate = outCandidates.emplace_back();
        candidate.candidate = CharSpan(buffered.candidate.data(), buffered.candidate.size());
        if (buffered.sdpMid.has_value())
        {
            candidate.SDPMid.SetNonNull(CharSpan(buffered.sdpMid->data(), buffered.sdpMid->size()));
        }
        else
        {
            candidate.SDPMid.SetNull();
        }
        candidate.SDPMLineIndex = buffered.sdpMLineIndex;
    }

    outWireSessionId = request.sessionId;
    CHIP_FAULT_INJECT(chip::FaultInjection::kFault_ModifyWebRTCICECandidatesSessionId, outWireSessionId++);
    CHIP_FAULT_INJECT(chip::FaultInjection::kFault_EmptyWebRTCICECandidatesList, outCandidates.clear());

    outCmd.webRTCSessionID = outWireSessionId;
    if (outCandidates.empty())
    {
        outCmd.ICECandidates = DataModel::List<const Clusters::Globals::Structs::ICECandidateStruct::Type>();
    }
    else
    {
        outCmd.ICECandidates =
            DataModel::List<const Clusters::Globals::Structs::ICECandidateStruct::Type>(outCandidates.data(), outCandidates.size());
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR WebRTCTransportRequestorClient::BuildEnd(const PendingRequest & request,
                                                    Clusters::WebRTCTransportRequestor::Commands::End::Type & outCmd) const
{
    outCmd.webRTCSessionID = request.sessionId;
    outCmd.reason          = request.endReason;
    return CHIP_NO_ERROR;
}

CHIP_ERROR WebRTCTransportRequestorClient::SendCommand(PendingRequest & request, Messaging::ExchangeManager & exchangeMgr,
                                                       const SessionHandle & sessionHandle)
{
    switch (request.commandType)
    {
    case CommandType::kOffer: {
        uint16_t wireSessionId = 0;
        Clusters::WebRTCTransportRequestor::Commands::Offer::Type cmd;
        ReturnErrorOnFailure(BuildOffer(request, wireSessionId, cmd));
        return InvokeOnSession(request, Clusters::WebRTCTransportRequestor::Commands::Offer::Id,
                               DataModel::EncodableType<Clusters::WebRTCTransportRequestor::Commands::Offer::Type>(cmd),
                               exchangeMgr, sessionHandle);
    }
    case CommandType::kAnswer: {
        uint16_t wireSessionId = 0;
        Clusters::WebRTCTransportRequestor::Commands::Answer::Type cmd;
        ReturnErrorOnFailure(BuildAnswer(request, wireSessionId, cmd));
        return InvokeOnSession(request, Clusters::WebRTCTransportRequestor::Commands::Answer::Id,
                               DataModel::EncodableType<Clusters::WebRTCTransportRequestor::Commands::Answer::Type>(cmd),
                               exchangeMgr, sessionHandle);
    }
    case CommandType::kICECandidates: {
        uint16_t wireSessionId = 0;
        std::vector<Clusters::Globals::Structs::ICECandidateStruct::Type> candidates;
        Clusters::WebRTCTransportRequestor::Commands::ICECandidates::Type cmd;
        ReturnErrorOnFailure(BuildICECandidates(request, wireSessionId, candidates, cmd));
        return InvokeOnSession(request, Clusters::WebRTCTransportRequestor::Commands::ICECandidates::Id,
                               DataModel::EncodableType<Clusters::WebRTCTransportRequestor::Commands::ICECandidates::Type>(cmd),
                               exchangeMgr, sessionHandle);
    }
    case CommandType::kEnd: {
        Clusters::WebRTCTransportRequestor::Commands::End::Type cmd;
        ReturnErrorOnFailure(BuildEnd(request, cmd));
        return InvokeOnSession(request, Clusters::WebRTCTransportRequestor::Commands::End::Id,
                               DataModel::EncodableType<Clusters::WebRTCTransportRequestor::Commands::End::Type>(cmd), exchangeMgr,
                               sessionHandle);
    }
    }
    return CHIP_ERROR_INVALID_ARGUMENT;
}

CHIP_ERROR WebRTCTransportRequestorClient::InvokeOnSession(PendingRequest & request, CommandId commandId,
                                                           const DataModel::EncodableToTLV & payload,
                                                           Messaging::ExchangeManager & exchangeMgr,
                                                           const SessionHandle & sessionHandle)
{
    VerifyOrReturnError(!request.commandSender, CHIP_ERROR_INCORRECT_STATE);

    request.commandSender = Platform::MakeUnique<CommandSender>(&request, &exchangeMgr, sessionHandle,
                                                                /* aIsTimedRequest = */ false, /* aSuppressResponse = */ false,
                                                                sessionHandle->AllowsLargePayload());
    VerifyOrReturnError(request.commandSender != nullptr, CHIP_ERROR_NO_MEMORY);

    CommandPathParams commandPath{ request.endpoint, Clusters::WebRTCTransportRequestor::Id, commandId,
                                   CommandPathFlags::kEndpointIdValid };
    CommandSender::AddRequestDataParameters addParams;
    CHIP_ERROR err = request.commandSender->AddRequestData(commandPath, payload, addParams);
    if (err == CHIP_NO_ERROR)
    {
        err = request.commandSender->SendCommandRequest(sessionHandle);
    }
    if (err != CHIP_NO_ERROR)
    {
        request.commandSender.reset();
    }
    return err;
}

} // namespace webrtc
} // namespace app
} // namespace chip
