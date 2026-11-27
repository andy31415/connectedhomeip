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

#include <lib/core/CHIPError.h>
#include <lib/support/Span.h>

#include <cstdint>
#include <memory>
#include <optional>

namespace chip {
namespace app {
namespace webrtc {

enum class SdpType : uint8_t
{
    kOffer,
    kAnswer,
    kPranswer,
    kRollback,
};

enum class IceTransportPolicy : uint8_t
{
    kAll,
    kRelay,
};

struct IceCandidate
{
    CharSpan candidate;
    std::optional<CharSpan> sdpMid;
    std::optional<uint16_t> sdpMLineIndex;
};

struct IceServer
{
    Span<const CharSpan> urls;
    CharSpan username;
    CharSpan credential;
};

struct SFrameConfig
{
    uint16_t cipherSuite = 0;
    ByteSpan baseKey;
    ByteSpan kid;
};

/**
 * One WebRTC peer connection for a single session.
 *
 * Threading: All methods and Events callbacks run on the Matter thread. Implementations must
 * marshal underlying library callbacks onto the Matter thread before invoking Events, and must
 * guarantee that no Events callback fires after Close() returns.
 *
 * Lifetime: SDP and candidate spans passed into methods or delivered via Events are only valid
 * for the duration of the call; callers and implementations must copy any data they retain.
 */
class PeerConnection
{
public:
    class Events
    {
    public:
        virtual ~Events()                                             = default;
        virtual void OnLocalDescription(SdpType type, CharSpan sdp)   = 0;
        virtual void OnLocalCandidate(const IceCandidate & candidate) = 0;
        virtual void OnConnected()                                    = 0;
        virtual void OnClosed()                                       = 0;
    };

    virtual ~PeerConnection()                                             = default;
    virtual CHIP_ERROR CreateOffer()                                      = 0;
    virtual CHIP_ERROR SetRemoteDescription(SdpType type, CharSpan sdp)   = 0;
    virtual CHIP_ERROR AddRemoteCandidate(const IceCandidate & candidate) = 0;
    virtual void Close()                                                  = 0;
};

class PeerConnectionFactory
{
public:
    struct SessionConfig
    {
        uint16_t sessionId = 0;
        Span<const IceServer> iceServers; // deep-copied before Create() returns
        std::optional<IceTransportPolicy> iceTransportPolicy;
        std::optional<SFrameConfig> sFrameConfig;
        Span<const uint16_t> videoStreamIds;
        Span<const uint16_t> audioStreamIds;
    };

    virtual ~PeerConnectionFactory()                                                                              = default;
    virtual std::unique_ptr<PeerConnection> Create(const SessionConfig & config, PeerConnection::Events & events) = 0;
};

} // namespace webrtc
} // namespace app
} // namespace chip
