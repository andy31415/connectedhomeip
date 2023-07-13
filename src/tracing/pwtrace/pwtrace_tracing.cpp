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

#include <fcntl.h>
#include <unistd.h>

// Define trace time for pw_trace
PW_TRACE_TIME_TYPE pw_trace_GetTraceTime()
{
    return (PW_TRACE_TIME_TYPE) chip::System::SystemClock().GetMonotonicMicroseconds64().count();
}

// Microsecond time source
size_t pw_trace_GetTraceTimeTicksPerSecond()
{
    return 1000000;
}

namespace chip {
namespace Tracing {
namespace PwTrace {

namespace {

void TraceSinkStartBlock(void * user_data, size_t size)
{
    int fileId = *reinterpret_cast<int *>(user_data);
    if (fileId >= 0) {
        // Each block is prefixed by size
        uint8_t block_size = static_cast<uint8_t>(size);
        write(fileId, &block_size, sizeof(block_size));
    }
}

void TraceSinkAddBytes(void * user_data, const void * bytes, size_t size)
{
    int fileId = *reinterpret_cast<int *>(user_data);
    if (fileId >= 0) {
        write(fileId, bytes, size);
    }
}

void TraceSinkEndBlock(void * user_data)
{
    // int fileId = *reinterpret_cast<int *>(user_data);
    // We could fsync here, however for performance purposes we do not
    // bother.
}

} // namespace

void PwTraceBackend::Open()
{
    pw::trace::Callbacks::Instance()
        .RegisterSink(TraceSinkStartBlock, TraceSinkAddBytes, TraceSinkEndBlock, &mTraceFileId, &mSinkHandle)
        .IgnoreError();

    PW_TRACE_SET_ENABLED(true);
}

void PwTraceBackend::Close()
{
    PW_TRACE_SET_ENABLED(false);

    pw::trace::Callbacks::Instance().UnregisterSink(mSinkHandle).IgnoreError();

    if (mTraceFileId != kInvalidFileId)
    {
        close(mTraceFileId);
        mTraceFileId = kInvalidFileId;
    }
}

CHIP_ERROR PwTraceBackend::SetOutputFile(const char * file_name)
{
    if (mTraceFileId != kInvalidFileId)
    {
        close(mTraceFileId);
        mTraceFileId = kInvalidFileId;
    }

    // Create a trace file and start sending data to it
    mTraceFileId = open(file_name, O_RDWR | O_CREAT | O_TRUNC, 0640);
    if (mTraceFileId < 0)
    {
        mTraceFileId = kInvalidFileId;
        return CHIP_ERROR_POSIX(errno);
    }

    return CHIP_NO_ERROR;
}

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
