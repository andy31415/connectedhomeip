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

#include <tracing/pwtrace/pwtrace_tracing.h>

#include <lib/address_resolve/TracingStructs.h>
#include <lib/support/ErrorStr.h>
#include <lib/support/StringBuilder.h>
#include <transport/TracingStructs.h>

#include <matter/tracing/macros_impl.h>

namespace chip {
namespace Tracing {
namespace PwTrace {

void PwTraceBackend::LogMessageReceived(MessageReceivedInfo & info)
{
    switch (info.messageType)
    {
    case IncomingMessageType::kGroupMessage:
        PW_TRACE_INSTANT("Group", "Message Received");
        break;
    case IncomingMessageType::kSecureUnicast:
        PW_TRACE_INSTANT("Secure", "Message Received");
        break;
    case IncomingMessageType::kUnauthenticated:
        PW_TRACE_INSTANT("Unauthenticated", "Message Received");
        break;
    default:
        PW_TRACE_INSTANT("Unknown", "Message Received");
        break;
    }
}

void PwTraceBackend::LogMessageSend(MessageSendInfo & info)
{
    switch (info.messageType)
    {
    case OutgoingMessageType::kGroupMessage:
        PW_TRACE_INSTANT("Group", "Message Send");
        break;
    case OutgoingMessageType::kSecureSession:
        PW_TRACE_INSTANT("Secure", "Message Send");
        break;
    case OutgoingMessageType::kUnauthenticated:
        PW_TRACE_INSTANT("Unauthenticated", "Message Send");
        break;
    default:
        PW_TRACE_INSTANT("Unknown", "Message Send");
        break;
    }

}

void PwTraceBackend::LogNodeLookup(NodeLookupInfo & info)
{
    // TODO: use pw_py_struct formatting to see data
    PW_TRACE_INSTANT("NodeLookup", "DNSSD");
}

void PwTraceBackend::LogNodeDiscovered(NodeDiscoveredInfo & info)
{
    // TODO: use pw_py_struct formatting to see data
    char address_buff[chip::Transport::PeerAddress::kMaxToStringSize];
    info.result->address.ToString(address_buff);

    switch (info.type)
    {
    case chip::Tracing::DiscoveryInfoType::kIntermediateResult:
        PW_TRACE_INSTANT("NodeDiscovered Intermediate", "DNSSD");
        break;
    case chip::Tracing::DiscoveryInfoType::kResolutionDone:
        PW_TRACE_INSTANT("NodeDiscovered Final", "DNSSD");
        break;
    case chip::Tracing::DiscoveryInfoType::kRetryDifferent:
        PW_TRACE_INSTANT("NodeDiscovered Retry Different", "DNSSD");
        break;
    }
}

void PwTraceBackend::LogNodeDiscoveryFailed(NodeDiscoveryFailedInfo & info)
{
    // TODO: use pw_py_struct formatting to see data
    PW_TRACE_INSTANT("Discovery Failed", "DNSSD");
}

} // namespace PwTrace
} // namespace Tracing
} // namespace chip
