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
#include <app/MessageDef/StatusIB.h>
#include <app/webrtc/WebRTCTransportRequestorClient.h>
#include <lib/core/CHIPCore.h>
#include <lib/support/CHIPFaultInjection.h>

#include <optional>
#include <string>
#include <vector>

namespace {

using namespace chip;
using namespace chip::app;
using namespace chip::app::webrtc;
using namespace chip::Protocols::InteractionModel;

const ScopedNodeId kPeerA(0x1111, 1);
const ScopedNodeId kPeerB(0x2222, 2);
constexpr EndpointId kEndpointA = 1;
constexpr EndpointId kEndpointB = 3;

class RecordingCallback : public WebRTCTransportRequestorClient::Callback
{
public:
    struct Result
    {
        uint16_t sessionId;
        CHIP_ERROR err;
        Status status;
    };

    void OnCommandResult(uint16_t sessionId, CHIP_ERROR err, Status status) override
    {
        results.push_back({ sessionId, err, status });
    }

    std::vector<Result> results;
};

class InterceptingRequestorClient : public WebRTCTransportRequestorClient
{
public:
    using WebRTCTransportRequestorClient::BuildAnswer;
    using WebRTCTransportRequestorClient::BuildEnd;
    using WebRTCTransportRequestorClient::BuildICECandidates;
    using WebRTCTransportRequestorClient::BuildOffer;
    using WebRTCTransportRequestorClient::PendingRequest;
    using WebRTCTransportRequestorClient::PendingRequests;
    using WebRTCTransportRequestorClient::WebRTCTransportRequestorClient;

    struct SentRecord
    {
        PendingRequest * request = nullptr;
        CommandType type;
        uint16_t wireSessionId = 0;
        std::string sdp;
        std::vector<std::string> candidates;
        std::vector<std::optional<std::string>> sdpMids;
        std::vector<DataModel::Nullable<uint16_t>> sdpMLineIndexes;
        Clusters::Globals::WebRTCEndReasonEnum endReason = Clusters::Globals::WebRTCEndReasonEnum::kUnknownEnumValue;
    };

    void EstablishSession(PendingRequest & request) override
    {
        establishedPeers.push_back(request.peer);
        queuedRequests.push_back(&request);
        if (autoConnectSync)
        {
            SimulateConnected(&request);
        }
    }

    void SimulateConnected(PendingRequest * request)
    {
        request->commandSender = Platform::MakeUnique<CommandSender>(nullptr, nullptr, std::nullopt);
        ASSERT_NE(request->commandSender.get(), nullptr);

        SentRecord record;
        record.request = request;
        record.type    = request->commandType;
        switch (request->commandType)
        {
        case CommandType::kOffer: {
            Clusters::WebRTCTransportRequestor::Commands::Offer::Type cmd;
            EXPECT_SUCCESS(BuildOffer(*request, record.wireSessionId, cmd));
            record.sdp.assign(cmd.sdp.data(), cmd.sdp.size());
            break;
        }
        case CommandType::kAnswer: {
            Clusters::WebRTCTransportRequestor::Commands::Answer::Type cmd;
            EXPECT_SUCCESS(BuildAnswer(*request, record.wireSessionId, cmd));
            record.sdp.assign(cmd.sdp.data(), cmd.sdp.size());
            break;
        }
        case CommandType::kICECandidates: {
            std::vector<Clusters::Globals::Structs::ICECandidateStruct::Type> wireCandidates;
            Clusters::WebRTCTransportRequestor::Commands::ICECandidates::Type cmd;
            EXPECT_SUCCESS(BuildICECandidates(*request, record.wireSessionId, wireCandidates, cmd));
            for (const auto & c : cmd.ICECandidates)
            {
                record.candidates.emplace_back(c.candidate.data(), c.candidate.size());
                record.sdpMids.push_back(c.SDPMid.IsNull()
                                             ? std::nullopt
                                             : std::make_optional(std::string(c.SDPMid.Value().data(), c.SDPMid.Value().size())));
                record.sdpMLineIndexes.push_back(c.SDPMLineIndex);
            }
            break;
        }
        case CommandType::kEnd: {
            Clusters::WebRTCTransportRequestor::Commands::End::Type cmd;
            EXPECT_SUCCESS(BuildEnd(*request, cmd));
            record.wireSessionId = cmd.webRTCSessionID;
            record.endReason     = cmd.reason;
            break;
        }
        }
        sentRecords.push_back(std::move(record));
    }

    void SimulateCaseFailure(PendingRequest * request, CHIP_ERROR err)
    {
        request->onConnectionFailure.mCall(request->onConnectionFailure.mContext, request->peer, err);
    }

    void CompleteWithResponse(PendingRequest * request, Status status)
    {
        CommandSender * sender = request->commandSender.get();
        ConcreteCommandPath path(request->endpoint, Clusters::WebRTCTransportRequestor::Id, 0);
        StatusIB statusIB(status);
        CommandSender::ResponseData response{ path, statusIB, nullptr, NullOptional };
        request->OnResponse(sender, response);
        request->OnDone(sender);
    }

    void CompleteWithError(PendingRequest * request, CHIP_ERROR err)
    {
        CommandSender * sender = request->commandSender.get();
        CommandSender::ErrorData errorData{ err };
        request->OnError(sender, errorData);
        request->OnDone(sender);
    }

    bool autoConnectSync = true;
    std::vector<ScopedNodeId> establishedPeers;
    std::vector<PendingRequest *> queuedRequests;
    std::vector<SentRecord> sentRecords;
};

struct TestWebRTCTransportRequestorClient : public ::testing::Test
{
    static void SetUpTestSuite() { ASSERT_SUCCESS(Platform::MemoryInit()); }
    static void TearDownTestSuite() { Platform::MemoryShutdown(); }

    CASESessionManager caseSessionManager;
};

TEST_F(TestWebRTCTransportRequestorClient, ValidationRejectsInvalidInputs)
{
    InterceptingRequestorClient client(caseSessionManager);
    RecordingCallback cb;

    EXPECT_EQ(client.SendOffer(ScopedNodeId(), kEndpointA, 1, "v=0"_span, &cb), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(client.SendOffer(kPeerA, kInvalidEndpointId, 1, "v=0"_span, &cb), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(client.SendOffer(kPeerA, kEndpointA, 1, ""_span, &cb), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(client.SendAnswer(kPeerA, kEndpointA, 1, ""_span, &cb), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(client.SendICECandidates(kPeerA, kEndpointA, 1, {}, &cb), CHIP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(client.SendEnd(kPeerA, kEndpointA, 1, Clusters::Globals::WebRTCEndReasonEnum::kUnknownEnumValue, &cb),
              CHIP_ERROR_INVALID_ARGUMENT);

    EXPECT_EQ(client.PendingRequestCount(), 0u);
    EXPECT_TRUE(cb.results.empty());
}

TEST_F(TestWebRTCTransportRequestorClient, ConcurrentSessionsAndPipelinedCommands)
{
    InterceptingRequestorClient client(caseSessionManager);
    RecordingCallback cb;

    Clusters::Globals::Structs::ICECandidateStruct::Type cand;
    cand.candidate = "candidate:1 1 UDP 1 192.0.2.1 5000 typ host"_span;
    cand.SDPMid.SetNonNull("video"_span);
    cand.SDPMLineIndex.SetNonNull(0);

    ASSERT_SUCCESS(client.SendOffer(kPeerA, kEndpointA, 10, "v=0 offer10"_span, &cb));
    ASSERT_SUCCESS(client.SendICECandidates(kPeerA, kEndpointA, 10, Span(&cand, 1), &cb));
    ASSERT_SUCCESS(client.SendAnswer(kPeerB, kEndpointB, 20, "v=0 answer20"_span, &cb));

    ASSERT_EQ(client.PendingRequestCount(), 3u);
    ASSERT_EQ(client.sentRecords.size(), 3u);

    auto * reqOffer10 = client.sentRecords[0].request;
    auto * reqIce10   = client.sentRecords[1].request;
    auto * reqAns20   = client.sentRecords[2].request;

    // Complete out of order
    client.CompleteWithResponse(reqAns20, Status::Success);
    client.CompleteWithResponse(reqIce10, Status::Success);
    client.CompleteWithResponse(reqOffer10, Status::Success);

    EXPECT_EQ(client.PendingRequestCount(), 0u);
    ASSERT_EQ(cb.results.size(), 3u);
    EXPECT_EQ(cb.results[0].sessionId, 20u);
    EXPECT_EQ(cb.results[0].status, Status::Success);
    EXPECT_EQ(cb.results[1].sessionId, 10u);
    EXPECT_EQ(cb.results[1].status, Status::Success);
    EXPECT_EQ(cb.results[2].sessionId, 10u);
    EXPECT_EQ(cb.results[2].status, Status::Success);
}

TEST_F(TestWebRTCTransportRequestorClient, CandidateNullabilityAndEndPayload)
{
    InterceptingRequestorClient client(caseSessionManager);

    Clusters::Globals::Structs::ICECandidateStruct::Type cands[2];
    cands[0].candidate = "cand0"_span;
    cands[0].SDPMid.SetNonNull("0"_span);
    cands[0].SDPMLineIndex.SetNonNull(0);

    cands[1].candidate = "cand1"_span;
    cands[1].SDPMid.SetNull();
    cands[1].SDPMLineIndex.SetNull();

    ASSERT_SUCCESS(client.SendICECandidates(kPeerA, kEndpointA, 42, Span(cands, 2)));
    ASSERT_SUCCESS(client.SendEnd(kPeerA, kEndpointA, 42, Clusters::Globals::WebRTCEndReasonEnum::kPrivacyMode));

    ASSERT_EQ(client.sentRecords.size(), 2u);
    const auto & iceRec = client.sentRecords[0];
    EXPECT_EQ(iceRec.wireSessionId, 42u);
    ASSERT_EQ(iceRec.candidates.size(), 2u);
    EXPECT_EQ(iceRec.candidates[0], "cand0");
    ASSERT_TRUE(iceRec.sdpMids[0].has_value());
    EXPECT_EQ(*iceRec.sdpMids[0], "0");
    EXPECT_FALSE(iceRec.sdpMLineIndexes[0].IsNull());
    EXPECT_EQ(iceRec.sdpMLineIndexes[0].Value(), 0u);

    EXPECT_EQ(iceRec.candidates[1], "cand1");
    EXPECT_FALSE(iceRec.sdpMids[1].has_value());
    EXPECT_TRUE(iceRec.sdpMLineIndexes[1].IsNull());

    const auto & endRec = client.sentRecords[1];
    EXPECT_EQ(endRec.wireSessionId, 42u);
    EXPECT_EQ(endRec.endReason, Clusters::Globals::WebRTCEndReasonEnum::kPrivacyMode);

    client.CancelAll();
}

TEST_F(TestWebRTCTransportRequestorClient, CaseFailureAndImErrors)
{
    InterceptingRequestorClient client(caseSessionManager);
    client.autoConnectSync = false;
    RecordingCallback cb;

    ASSERT_SUCCESS(client.SendOffer(kPeerA, kEndpointA, 1, "v=0"_span, &cb));
    ASSERT_EQ(client.queuedRequests.size(), 1u);
    client.SimulateCaseFailure(client.queuedRequests[0], CHIP_ERROR_TIMEOUT);

    EXPECT_EQ(client.PendingRequestCount(), 0u);
    ASSERT_EQ(cb.results.size(), 1u);
    EXPECT_EQ(cb.results[0].sessionId, 1u);
    EXPECT_EQ(cb.results[0].err, CHIP_ERROR_TIMEOUT);
    EXPECT_EQ(cb.results[0].status, Status::Failure);

    client.autoConnectSync = true;
    ASSERT_SUCCESS(client.SendAnswer(kPeerA, kEndpointA, 2, "v=0"_span, &cb));
    ASSERT_SUCCESS(client.SendEnd(kPeerA, kEndpointA, 3, Clusters::Globals::WebRTCEndReasonEnum::kUserHangup, &cb));

    client.CompleteWithResponse(client.sentRecords[0].request, Status::NotFound);
    client.CompleteWithError(client.sentRecords[1].request, StatusIB(Status::Busy).ToChipError());

    ASSERT_EQ(cb.results.size(), 3u);
    EXPECT_EQ(cb.results[1].sessionId, 2u);
    EXPECT_EQ(cb.results[1].status, Status::NotFound);
    EXPECT_EQ(cb.results[2].sessionId, 3u);
    EXPECT_EQ(cb.results[2].status, Status::Busy);
}

TEST_F(TestWebRTCTransportRequestorClient, CancelForSessionPreservesOtherSessions)
{
    InterceptingRequestorClient client(caseSessionManager);
    RecordingCallback cb;

    Clusters::Globals::Structs::ICECandidateStruct::Type cand;
    cand.candidate = "cand"_span;
    cand.SDPMid.SetNull();
    cand.SDPMLineIndex.SetNull();

    ASSERT_SUCCESS(client.SendOffer(kPeerA, kEndpointA, 10, "v=0"_span, &cb));
    ASSERT_SUCCESS(client.SendICECandidates(kPeerA, kEndpointA, 10, Span(&cand, 1), &cb));
    ASSERT_SUCCESS(client.SendAnswer(kPeerB, kEndpointB, 20, "v=0"_span, &cb));
    EXPECT_EQ(client.PendingRequestCount(), 3u);

    client.CancelForSession(10);
    EXPECT_EQ(client.PendingRequestCount(), 1u);

    client.CompleteWithResponse(client.sentRecords[2].request, Status::Success);
    EXPECT_EQ(client.PendingRequestCount(), 0u);
    ASSERT_EQ(cb.results.size(), 1u);
    EXPECT_EQ(cb.results[0].sessionId, 20u);
    EXPECT_EQ(cb.results[0].status, Status::Success);
}

#if CHIP_WITH_NLFAULTINJECTION
TEST_F(TestWebRTCTransportRequestorClient, FaultInjectionModifiesWirePayloads)
{
    InterceptingRequestorClient client(caseSessionManager);
    RecordingCallback cb;

    auto & faultMgr = chip::FaultInjection::GetManager();
    faultMgr.FailAtFault(chip::FaultInjection::kFault_ModifyWebRTCOfferSessionId, 0, 1);
    faultMgr.FailAtFault(chip::FaultInjection::kFault_ModifyWebRTCAnswerSessionId, 0, 1);
    faultMgr.FailAtFault(chip::FaultInjection::kFault_ModifyWebRTCICECandidatesSessionId, 0, 1);
    faultMgr.FailAtFault(chip::FaultInjection::kFault_EmptyWebRTCICECandidatesList, 0, 1);

    Clusters::Globals::Structs::ICECandidateStruct::Type cand;
    cand.candidate = "cand"_span;
    cand.SDPMid.SetNull();
    cand.SDPMLineIndex.SetNull();

    ASSERT_SUCCESS(client.SendOffer(kPeerA, kEndpointA, 5, "v=0"_span, &cb));
    ASSERT_SUCCESS(client.SendAnswer(kPeerA, kEndpointA, 6, "v=0"_span, &cb));
    ASSERT_SUCCESS(client.SendICECandidates(kPeerA, kEndpointA, 7, Span(&cand, 1), &cb));

    ASSERT_EQ(client.sentRecords.size(), 3u);
    EXPECT_EQ(client.sentRecords[0].wireSessionId, 6u);
    EXPECT_EQ(client.sentRecords[1].wireSessionId, 7u);
    EXPECT_EQ(client.sentRecords[2].wireSessionId, 8u);
    EXPECT_TRUE(client.sentRecords[2].candidates.empty());

    // Internal sessionId reported to callback must remain unmodified.
    client.CompleteWithResponse(client.sentRecords[0].request, Status::Success);
    client.CompleteWithResponse(client.sentRecords[1].request, Status::Success);
    client.CompleteWithResponse(client.sentRecords[2].request, Status::Success);

    ASSERT_EQ(cb.results.size(), 3u);
    EXPECT_EQ(cb.results[0].sessionId, 5u);
    EXPECT_EQ(cb.results[1].sessionId, 6u);
    EXPECT_EQ(cb.results[2].sessionId, 7u);
}
#endif // CHIP_WITH_NLFAULTINJECTION

} // namespace
