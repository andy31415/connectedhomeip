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

#include "Esp32MediaControl.h"

#include <app/webrtc/PeerConnection.h>
#include <esp_peer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace chip::app {

/**
 * @brief ESP32 PeerConnection backed by espressif/esp_peer.
 *
 * Lazily opens the underlying esp_peer session once the signaling role is known
 * (CONTROLLING on CreateOffer, CONTROLLED on SetRemoteDescription(kOffer)) and
 * marshals all esp_peer callbacks onto the Matter thread via ScheduleLambda.
 */
class Esp32PeerConnection : public webrtc::PeerConnection, public Esp32MediaControl::VideoFrameSink
{
public:
    struct CallbackState
    {
        explicit CallbackState(webrtc::PeerConnection::Events & eventsRef) : events(eventsRef) {}

        std::atomic<bool> closed{ false };
        esp_peer_role_t role       = ESP_PEER_ROLE_CONTROLLING;
        Esp32PeerConnection * self = nullptr;
        std::weak_ptr<CallbackState> weakSelf;
        webrtc::PeerConnection::Events & events;
    };

    Esp32PeerConnection(const webrtc::PeerConnectionFactory::SessionConfig & config, webrtc::PeerConnection::Events & events,
                        Esp32MediaControl & mediaControl);
    ~Esp32PeerConnection() override;

    // webrtc::PeerConnection
    CHIP_ERROR CreateOffer() override;
    CHIP_ERROR SetRemoteDescription(webrtc::SdpType type, CharSpan sdp) override;
    CHIP_ERROR AddRemoteCandidate(const webrtc::IceCandidate & candidate) override;
    void Close() override;

    // Esp32MediaControl::VideoFrameSink (called on the capture task thread)
    void OnVideoFrame(uint32_t ptsMs, const uint8_t * data, size_t size, bool isKeyframe) override;

    void OnPeerConnectedOnMatterThread();
    void OnPeerClosedOnMatterThread();

private:
    struct IceServerStorage
    {
        std::string url;
        std::string username;
        std::string credential;
    };

    CHIP_ERROR OpenPeerLocked(esp_peer_role_t role);
    void FlushPendingCandidatesLocked();

    static int HandlePeerState(esp_peer_state_t state, void * ctx);
    static int HandlePeerMsg(esp_peer_msg_t * msg, void * ctx);
    static void LoopTaskEntry(void * arg);
    void RunPeerLoop();

    Esp32MediaControl & mMediaControl;
    std::shared_ptr<CallbackState> mState;

    // Deep-copied session configuration
    uint16_t mSessionId                            = 0;
    webrtc::IceTransportPolicy mIceTransportPolicy = webrtc::IceTransportPolicy::kAll;
    bool mSendVideo                                = false;
    std::vector<IceServerStorage> mIceServerStorage;
    std::vector<esp_peer_ice_server_cfg_t> mIceServerCfgs;
    std::vector<std::string> mPendingCandidates;

    std::mutex mPeerMutex;
    esp_peer_handle_t mPeer = nullptr;

    std::atomic<bool> mLoopRunning{ false };
    std::atomic<bool> mPeerConnected{ false };
    TaskHandle_t mLoopTaskHandle   = nullptr;
    StackType_t * mLoopTaskStack   = nullptr;
    StaticTask_t * mLoopTaskTcb    = nullptr;
    SemaphoreHandle_t mLoopDoneSem = nullptr;
};

/**
 * @brief PeerConnectionFactory creating Esp32PeerConnection instances bound to Esp32MediaControl.
 */
class Esp32PeerConnectionFactory : public webrtc::PeerConnectionFactory
{
public:
    explicit Esp32PeerConnectionFactory(Esp32MediaControl & mediaControl);
    ~Esp32PeerConnectionFactory() override = default;

    std::unique_ptr<webrtc::PeerConnection> Create(const SessionConfig & config, webrtc::PeerConnection::Events & events) override;

private:
    Esp32MediaControl & mMediaControl;
    bool mCertPreGenerated = false;
};

} // namespace chip::app
