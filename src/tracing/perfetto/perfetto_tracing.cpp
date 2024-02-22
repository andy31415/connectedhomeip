/*
 *
 *    Copyright (c) 2023 Project CHIP Authors
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

#include <tracing/perfetto/perfetto_tracing.h>

#include <lib/address_resolve/TracingStructs.h>
#include <lib/core/ErrorStr.h>
#include <lib/support/StringBuilder.h>
#include <matter/tracing/macros_impl.h>
#include <perfetto.h>
#include <transport/TracingStructs.h>

namespace chip {
namespace Tracing {
namespace Perfetto {

void PerfettoBackend::LogMessageReceived(const MessageReceivedInfo & info)
{
    const char * messageType = "UNKNOWN";
    switch (info.messageType)
    {
    case IncomingMessageType::kGroupMessage:
        messageType = "Group";
        break;
    case IncomingMessageType::kSecureUnicast:
        messageType = "Secure";
        break;
    case IncomingMessageType::kUnauthenticated:
        messageType = "Unauthenticated";
        break;
    }

    TRACE_EVENT_INSTANT(              //
        "Matter", "Message Received", //
        "message_type", messageType   //
    );
}

void PerfettoBackend::LogMessageSend(const MessageSendInfo & info)
{
    const char * messageType = "UNKNOWN";
    switch (info.messageType)
    {
    case OutgoingMessageType::kGroupMessage:
        messageType = "Group";
        break;
    case OutgoingMessageType::kSecureSession:
        messageType = "Secure";
        break;
    case OutgoingMessageType::kUnauthenticated:
        messageType = "Unauthenticated";
        break;
    }

    TRACE_EVENT_INSTANT(            //
        "Matter", "Message Send",   //
        "message_type", messageType //
    );
}

void PerfettoBackend::LogNodeLookup(const NodeLookupInfo & info)
{
    TRACE_EVENT_INSTANT(                                                          //
        "Matter", "NodeLookup",                                                   //
        "node_id", info.request->GetPeerId().GetNodeId(),                         //
        "compressed_fabric_id", info.request->GetPeerId().GetCompressedFabricId() //
    );
}

void PerfettoBackend::LogNodeDiscovered(const NodeDiscoveredInfo & info)
{
    char address_buff[chip::Transport::PeerAddress::kMaxToStringSize];
    info.result->address.ToString(address_buff);

    switch (info.type)
    {
    case chip::Tracing::DiscoveryInfoType::kIntermediateResult:
        TRACE_EVENT_INSTANT(                                              //
            "Matter", "NodeDiscovered Intermediate",                      //
            "node_id", info.peerId->GetNodeId(),                          //
            "compressed_fabric_id", info.peerId->GetCompressedFabricId(), //
            "address", address_buff                                       //
        );
        break;
    case chip::Tracing::DiscoveryInfoType::kResolutionDone:
        TRACE_EVENT_INSTANT(                                              //
            "Matter", "NodeDiscovered Final",                             //
            "node_id", info.peerId->GetNodeId(),                          //
            "compressed_fabric_id", info.peerId->GetCompressedFabricId(), //
            "address", address_buff                                       //
        );
        break;
    case chip::Tracing::DiscoveryInfoType::kRetryDifferent:
        TRACE_EVENT_INSTANT(                                              //
            "Matter", "NodeDiscovered Retry Different",                   //
            "node_id", info.peerId->GetNodeId(),                          //
            "compressed_fabric_id", info.peerId->GetCompressedFabricId(), //
            "address", address_buff                                       //
        );
        break;
    }
}

void PerfettoBackend::LogNodeDiscoveryFailed(const NodeDiscoveryFailedInfo & info)
{
    TRACE_EVENT_INSTANT(                                              //
        "Matter", "Discovery Failed",                                 //
        "node_id", info.peerId->GetNodeId(),                          //
        "compressed_fabric_id", info.peerId->GetCompressedFabricId(), //
        "error", chip::ErrorStr(info.error)                           //
    );
}

void PerfettoBackend::LogMetric(const char * label, const Metric & metric)
{
    switch (metric.GetType())
    {
    case Metric::Type::kInt32:
        TRACE_EVENT_INSTANT("Matter", label, "value", metric.ValueInt32());
        break;
    case Metric::Type::kUInt32:
        TRACE_EVENT_INSTANT("Matter", label, "value", metric.ValueUInt32());
        break;
    case Metric::Type::kErrorCode:
        TRACE_EVENT_INSTANT("Matter", label, "error", metric.ValueErrorCode());
        break;
    case Metric::Type::kProcessExecution:
        switch (metric.ValueProcessExecution())
        {
        case ProcessExecution::kStart:
            TRACE_EVENT_INSTANT("Matter", label, "state", "START");
            break;
        case ProcessExecution::kHeartbeat:
            TRACE_EVENT_INSTANT("Matter", label, "state", "HEARTBEAT");
            break;
        case ProcessExecution::kEnd:
            TRACE_EVENT_INSTANT("Matter", label, "state", "END");
            break;
        default:
            TRACE_EVENT_INSTANT("Matter", label, "state", "UNKNOWN");
            break;
        }
        break;
    default:
        TRACE_EVENT_INSTANT("Matter", label, "type", "UNKNOWN");
        break;
    }
}

} // namespace Perfetto
} // namespace Tracing
} // namespace chip
