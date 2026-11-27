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

#include "Esp32PeerConnection.h"

#include <esp_heap_caps.h>
#include <esp_peer_default.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/CHIPDeviceLayer.h>

#include <algorithm>
#include <cstring>
#include <new>

namespace chip::app {

namespace {

constexpr uint32_t kPeerLoopStackSize   = 16384;
constexpr UBaseType_t kPeerLoopTaskPrio = 3;
constexpr BaseType_t kPeerLoopTaskCore  = 0;
constexpr uint16_t kAgentRecvTimeoutMs  = 20;
constexpr uint32_t kPeerLoopDelayMs     = 20;
constexpr uint32_t kRtpSendPoolBytes    = 128 * 1024;
constexpr uint32_t kRtpSendQueueNum     = 128;

struct PendingPeerEvent
{
    enum class Kind : uint8_t
    {
        kState,
        kMsg,
    };

    std::shared_ptr<Esp32PeerConnection::CallbackState> state;
    Kind kind                   = Kind::kState;
    esp_peer_state_t peerState  = ESP_PEER_STATE_CLOSED;
    esp_peer_msg_type_t msgType = ESP_PEER_MSG_TYPE_NONE;
    std::string payload;
};

void DispatchPendingEvent(PendingPeerEvent * evt)
{
    std::unique_ptr<PendingPeerEvent> owned(evt);
    if (owned == nullptr || owned->state == nullptr || owned->state->closed.load(std::memory_order_acquire))
    {
        return;
    }

    auto & state = owned->state;
    if (owned->kind == PendingPeerEvent::Kind::kState)
    {
        if (state->self == nullptr)
        {
            return;
        }
        if (owned->peerState == ESP_PEER_STATE_CONNECTED)
        {
            state->self->OnPeerConnectedOnMatterThread();
        }
        else
        {
            state->self->OnPeerClosedOnMatterThread();
        }
        return;
    }

    if (owned->msgType == ESP_PEER_MSG_TYPE_SDP)
    {
        const webrtc::SdpType sdpType =
            (state->role == ESP_PEER_ROLE_CONTROLLING) ? webrtc::SdpType::kOffer : webrtc::SdpType::kAnswer;
        state->events.OnLocalDescription(sdpType, CharSpan(owned->payload.data(), owned->payload.size()));
    }
    else if (owned->msgType == ESP_PEER_MSG_TYPE_CANDIDATE)
    {
        webrtc::IceCandidate candidate;
        candidate.candidate = CharSpan(owned->payload.data(), owned->payload.size());
        state->events.OnLocalCandidate(candidate);
    }
}

void SchedulePendingEvent(PendingPeerEvent * evt)
{
    if (evt == nullptr)
    {
        return;
    }
    // ScheduleLambda requires a trivially copyable lambda <= 24 bytes; pass only the raw pointer.
    CHIP_ERROR err = DeviceLayer::SystemLayer().ScheduleLambda([evt]() { DispatchPendingEvent(evt); });
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Camera, "Esp32PeerConnection: ScheduleLambda failed: %" CHIP_ERROR_FORMAT, err.Format());
        delete evt;
    }
}

std::string NormalizeRemoteCandidate(CharSpan raw)
{
    std::string candidate(raw.data(), raw.size());
    // esp_peer expects bare "candidate:..." without the "a=" SDP line prefix unless newline-terminated.
    if (candidate.rfind("a=", 0) == 0)
    {
        candidate.erase(0, 2);
    }
    while (!candidate.empty() && (candidate.back() == '\r' || candidate.back() == '\n'))
    {
        candidate.pop_back();
    }
    return candidate;
}

} // namespace

Esp32PeerConnection::Esp32PeerConnection(const webrtc::PeerConnectionFactory::SessionConfig & config,
                                         webrtc::PeerConnection::Events & events, Esp32MediaControl & mediaControl) :
    mMediaControl(mediaControl), mState(std::make_shared<CallbackState>(events)), mSessionId(config.sessionId),
    mIceTransportPolicy(config.iceTransportPolicy.value_or(webrtc::IceTransportPolicy::kAll)),
    mSendVideo(!config.videoStreamIds.empty())
{
    mState->self     = this;
    mState->weakSelf = mState;

    // Expand one esp_peer ICE entry per URL because esp_peer accepts one URL per entry.
    for (const auto & server : config.iceServers)
    {
        for (const auto & url : server.urls)
        {
            if (url.empty())
            {
                continue;
            }
            IceServerStorage entry;
            entry.url.assign(url.data(), url.size());
            entry.username.assign(server.username.data(), server.username.size());
            entry.credential.assign(server.credential.data(), server.credential.size());
            mIceServerStorage.push_back(std::move(entry));
        }
    }

    mIceServerCfgs.reserve(mIceServerStorage.size());
    for (auto & entry : mIceServerStorage)
    {
        esp_peer_ice_server_cfg_t cfg = {};
        cfg.stun_url                  = const_cast<char *>(entry.url.c_str());
        cfg.user                      = entry.username.empty() ? nullptr : const_cast<char *>(entry.username.c_str());
        cfg.psw                       = entry.credential.empty() ? nullptr : const_cast<char *>(entry.credential.c_str());
        mIceServerCfgs.push_back(cfg);
    }
}

Esp32PeerConnection::~Esp32PeerConnection()
{
    Close();
}

CHIP_ERROR Esp32PeerConnection::OpenPeerLocked(esp_peer_role_t role)
{
    if (mPeer != nullptr)
    {
        return CHIP_NO_ERROR;
    }

    mState->role = role;

    esp_peer_default_cfg_t defaultCfg = {};
    defaultCfg.agent_recv_timeout     = kAgentRecvTimeoutMs;
    defaultCfg.rtp_cfg.send_pool_size = kRtpSendPoolBytes;
    defaultCfg.rtp_cfg.send_queue_num = kRtpSendQueueNum;
    defaultCfg.ipv6_support           = true;
    defaultCfg.keep_role              = true;
    defaultCfg.max_candidates         = 8;

    esp_peer_cfg_t peerCfg   = {};
    peerCfg.server_lists     = mIceServerCfgs.empty() ? nullptr : mIceServerCfgs.data();
    peerCfg.server_num       = static_cast<uint8_t>(std::min<size_t>(mIceServerCfgs.size(), UINT8_MAX));
    peerCfg.role             = role;
    peerCfg.ice_trans_policy = (mIceTransportPolicy == webrtc::IceTransportPolicy::kRelay) ? ESP_PEER_ICE_TRANS_POLICY_RELAY
                                                                                           : ESP_PEER_ICE_TRANS_POLICY_ALL;
    peerCfg.audio_dir        = ESP_PEER_MEDIA_DIR_NONE;
    peerCfg.video_dir        = mSendVideo ? ESP_PEER_MEDIA_DIR_SEND_ONLY : ESP_PEER_MEDIA_DIR_NONE;
    if (mSendVideo)
    {
        peerCfg.video_info.codec  = ESP_PEER_VIDEO_CODEC_H264;
        peerCfg.video_info.width  = Esp32MediaControl::kVideoWidth;
        peerCfg.video_info.height = Esp32MediaControl::kVideoHeight;
        peerCfg.video_info.fps    = Esp32MediaControl::kTargetFps;
    }
    peerCfg.no_auto_reconnect = true;
    peerCfg.extra_cfg         = &defaultCfg;
    peerCfg.extra_size        = sizeof(defaultCfg);
    peerCfg.ctx               = mState.get();
    peerCfg.on_state          = HandlePeerState;
    peerCfg.on_msg            = HandlePeerMsg;

    const esp_peer_ops_t * ops = esp_peer_get_default_impl();
    VerifyOrReturnError(ops != nullptr, CHIP_ERROR_NO_MEMORY);

    int err = esp_peer_open(&peerCfg, ops, &mPeer);
    if (err != ESP_PEER_ERR_NONE || mPeer == nullptr)
    {
        ChipLogError(Camera, "Esp32PeerConnection[%u]: esp_peer_open failed: %d", mSessionId, err);
        mPeer = nullptr;
        return CHIP_ERROR_INTERNAL;
    }

    auto cleanupLoopResources = [&]() {
        if (mLoopDoneSem != nullptr)
        {
            vSemaphoreDelete(mLoopDoneSem);
            mLoopDoneSem = nullptr;
        }
        if (mLoopTaskStack != nullptr)
        {
            heap_caps_free(mLoopTaskStack);
            mLoopTaskStack = nullptr;
        }
        if (mLoopTaskTcb != nullptr)
        {
            heap_caps_free(mLoopTaskTcb);
            mLoopTaskTcb = nullptr;
        }
        if (mPeer != nullptr)
        {
            esp_peer_close(mPeer);
            mPeer = nullptr;
        }
    };

    mLoopDoneSem   = xSemaphoreCreateBinary();
    mLoopTaskStack = static_cast<StackType_t *>(heap_caps_malloc(kPeerLoopStackSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    mLoopTaskTcb   = static_cast<StaticTask_t *>(heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (mLoopDoneSem == nullptr || mLoopTaskStack == nullptr || mLoopTaskTcb == nullptr)
    {
        ChipLogError(Camera, "Esp32PeerConnection[%u]: Failed to allocate peer loop task memory", mSessionId);
        cleanupLoopResources();
        return CHIP_ERROR_NO_MEMORY;
    }

    mLoopRunning.store(true, std::memory_order_release);
    mLoopTaskHandle = xTaskCreateStaticPinnedToCore(LoopTaskEntry, "esp_peer", kPeerLoopStackSize / sizeof(StackType_t), this,
                                                    kPeerLoopTaskPrio, mLoopTaskStack, mLoopTaskTcb, kPeerLoopTaskCore);
    if (mLoopTaskHandle == nullptr)
    {
        mLoopRunning.store(false, std::memory_order_release);
        cleanupLoopResources();
        return CHIP_ERROR_NO_MEMORY;
    }

    return CHIP_NO_ERROR;
}

void Esp32PeerConnection::FlushPendingCandidatesLocked()
{
    VerifyOrReturn(mPeer != nullptr);
    for (auto & candidate : mPendingCandidates)
    {
        if (candidate.empty())
        {
            continue;
        }
        esp_peer_msg_t msg = {};
        msg.type           = ESP_PEER_MSG_TYPE_CANDIDATE;
        msg.data           = reinterpret_cast<uint8_t *>(candidate.data());
        msg.size           = static_cast<int>(candidate.size());
        int err            = esp_peer_send_msg(mPeer, &msg);
        if (err != ESP_PEER_ERR_NONE)
        {
            ChipLogError(Camera, "Esp32PeerConnection[%u]: Failed sending queued ICE candidate: %d", mSessionId, err);
        }
    }
    mPendingCandidates.clear();
}

CHIP_ERROR Esp32PeerConnection::CreateOffer()
{
    VerifyOrReturnError(!mState->closed.load(std::memory_order_acquire), CHIP_ERROR_INCORRECT_STATE);

    std::lock_guard<std::mutex> lock(mPeerMutex);
    CHIP_ERROR err = OpenPeerLocked(ESP_PEER_ROLE_CONTROLLING);
    if (err != CHIP_NO_ERROR)
    {
        return err;
    }

    int peerErr = esp_peer_new_connection(mPeer);
    if (peerErr != ESP_PEER_ERR_NONE)
    {
        ChipLogError(Camera, "Esp32PeerConnection[%u]: esp_peer_new_connection failed: %d", mSessionId, peerErr);
        return CHIP_ERROR_INTERNAL;
    }

    FlushPendingCandidatesLocked();
    return CHIP_NO_ERROR;
}

CHIP_ERROR Esp32PeerConnection::SetRemoteDescription(webrtc::SdpType type, CharSpan sdp)
{
    VerifyOrReturnError(!mState->closed.load(std::memory_order_acquire), CHIP_ERROR_INCORRECT_STATE);
    VerifyOrReturnError(type == webrtc::SdpType::kOffer || type == webrtc::SdpType::kAnswer, CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE);

    std::string sdpStr(sdp.data(), sdp.size());
    std::lock_guard<std::mutex> lock(mPeerMutex);

    if (mPeer == nullptr)
    {
        VerifyOrReturnError(type == webrtc::SdpType::kOffer, CHIP_ERROR_INCORRECT_STATE);
        ReturnErrorOnFailure(OpenPeerLocked(ESP_PEER_ROLE_CONTROLLED));
    }

    esp_peer_msg_t msg = {};
    msg.type           = ESP_PEER_MSG_TYPE_SDP;
    msg.data           = reinterpret_cast<uint8_t *>(sdpStr.data());
    msg.size           = static_cast<int>(sdpStr.size() + 1); // include NUL terminator as expected by esp_peer SDP parser
    int peerErr        = esp_peer_send_msg(mPeer, &msg);
    if (peerErr != ESP_PEER_ERR_NONE)
    {
        ChipLogError(Camera, "Esp32PeerConnection[%u]: esp_peer_send_msg(SDP) failed: %d", mSessionId, peerErr);
        return CHIP_ERROR_INTERNAL;
    }

    FlushPendingCandidatesLocked();
    return CHIP_NO_ERROR;
}

CHIP_ERROR Esp32PeerConnection::AddRemoteCandidate(const webrtc::IceCandidate & candidate)
{
    VerifyOrReturnError(!mState->closed.load(std::memory_order_acquire), CHIP_ERROR_INCORRECT_STATE);

    std::string normalized = NormalizeRemoteCandidate(candidate.candidate);
    if (normalized.empty())
    {
        return CHIP_NO_ERROR;
    }

    std::lock_guard<std::mutex> lock(mPeerMutex);
    if (mPeer == nullptr)
    {
        mPendingCandidates.push_back(std::move(normalized));
        return CHIP_NO_ERROR;
    }

    esp_peer_msg_t msg = {};
    msg.type           = ESP_PEER_MSG_TYPE_CANDIDATE;
    msg.data           = reinterpret_cast<uint8_t *>(normalized.data());
    msg.size           = static_cast<int>(normalized.size());
    int peerErr        = esp_peer_send_msg(mPeer, &msg);
    if (peerErr != ESP_PEER_ERR_NONE)
    {
        ChipLogError(Camera, "Esp32PeerConnection[%u]: esp_peer_send_msg(CANDIDATE) failed: %d", mSessionId, peerErr);
        return CHIP_ERROR_INTERNAL;
    }
    return CHIP_NO_ERROR;
}

void Esp32PeerConnection::Close()
{
    if (mState->closed.exchange(true, std::memory_order_acq_rel))
    {
        return;
    }

    mPeerConnected.store(false, std::memory_order_release);
    // Remove video sink BEFORE stopping the peer so DispatchVideoFrame cannot call into a closing peer.
    mMediaControl.RemoveVideoSink(this);

    esp_peer_handle_t peerToClose = nullptr;
    {
        std::lock_guard<std::mutex> lock(mPeerMutex);
        peerToClose = mPeer;
        if (peerToClose != nullptr)
        {
            esp_peer_disconnect(peerToClose);
        }
        mLoopRunning.store(false, std::memory_order_release);
    }

    if (mLoopTaskHandle != nullptr)
    {
        if (mLoopDoneSem != nullptr)
        {
            xSemaphoreTake(mLoopDoneSem, portMAX_DELAY);
        }
        mLoopTaskHandle = nullptr;
    }

    if (peerToClose != nullptr)
    {
        std::lock_guard<std::mutex> lock(mPeerMutex);
        esp_peer_close(peerToClose);
        mPeer = nullptr;
    }

    if (mLoopDoneSem != nullptr)
    {
        vSemaphoreDelete(mLoopDoneSem);
        mLoopDoneSem = nullptr;
    }
    if (mLoopTaskStack != nullptr)
    {
        heap_caps_free(mLoopTaskStack);
        mLoopTaskStack = nullptr;
    }
    if (mLoopTaskTcb != nullptr)
    {
        heap_caps_free(mLoopTaskTcb);
        mLoopTaskTcb = nullptr;
    }
}

void Esp32PeerConnection::OnVideoFrame(uint32_t ptsMs, const uint8_t * data, size_t size, bool /*isKeyframe*/)
{
    if (mState->closed.load(std::memory_order_acquire) || !mPeerConnected.load(std::memory_order_acquire) || data == nullptr ||
        size == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mPeerMutex);
    if (mPeer == nullptr)
    {
        return;
    }

    esp_peer_video_frame_t frame = {};
    frame.pts                    = ptsMs;
    frame.data                   = const_cast<uint8_t *>(data);
    frame.size                   = static_cast<int>(size);
    int err                      = esp_peer_send_video(mPeer, &frame);
    if (err != ESP_PEER_ERR_NONE && err != ESP_PEER_ERR_WOULD_BLOCK)
    {
        ChipLogDetail(Camera, "Esp32PeerConnection[%u]: esp_peer_send_video returned %d", mSessionId, err);
    }
}

void Esp32PeerConnection::LoopTaskEntry(void * arg)
{
    static_cast<Esp32PeerConnection *>(arg)->RunPeerLoop();
}

void Esp32PeerConnection::RunPeerLoop()
{
    while (mLoopRunning.load(std::memory_order_acquire))
    {
        {
            std::lock_guard<std::mutex> lock(mPeerMutex);
            if (mPeer == nullptr || !mLoopRunning.load(std::memory_order_acquire))
            {
                break;
            }
            esp_peer_main_loop(mPeer);
        }
        // Always sleep at least 1 RTOS tick (CONFIG_FREERTOS_HZ=100 makes <10ms round to 0 / taskYIELD).
        vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(kPeerLoopDelayMs)));
    }

    if (mLoopDoneSem != nullptr)
    {
        xSemaphoreGive(mLoopDoneSem);
    }
    vTaskDelete(nullptr);
}

void Esp32PeerConnection::OnPeerConnectedOnMatterThread()
{
    if (mState->closed.load(std::memory_order_acquire))
    {
        return;
    }
    mPeerConnected.store(true, std::memory_order_release);
    if (mSendVideo)
    {
        mMediaControl.AddVideoSink(this);
    }
    mState->events.OnConnected();
}

void Esp32PeerConnection::OnPeerClosedOnMatterThread()
{
    if (mState->closed.load(std::memory_order_acquire))
    {
        return;
    }
    mPeerConnected.store(false, std::memory_order_release);
    mMediaControl.RemoveVideoSink(this);
    mState->events.OnClosed();
}

int Esp32PeerConnection::HandlePeerState(esp_peer_state_t state, void * ctx)
{
    auto * rawState = static_cast<CallbackState *>(ctx);
    if (rawState == nullptr || rawState->closed.load(std::memory_order_acquire))
    {
        return 0;
    }

    auto sharedState = rawState->weakSelf.lock();
    if (sharedState == nullptr)
    {
        return 0;
    }

    if (state == ESP_PEER_STATE_VIDEO_PLI_RECEIVED)
    {
        if (sharedState->self != nullptr)
        {
            sharedState->self->mMediaControl.RequestKeyframe();
        }
        return 0;
    }

    if (state != ESP_PEER_STATE_CONNECTED && state != ESP_PEER_STATE_DISCONNECTED && state != ESP_PEER_STATE_CONNECT_FAILED &&
        state != ESP_PEER_STATE_CLOSED)
    {
        return 0;
    }

    auto * evt = new (std::nothrow) PendingPeerEvent();
    if (evt == nullptr)
    {
        return 0;
    }
    evt->state     = std::move(sharedState);
    evt->kind      = PendingPeerEvent::Kind::kState;
    evt->peerState = state;
    SchedulePendingEvent(evt);
    return 0;
}

int Esp32PeerConnection::HandlePeerMsg(esp_peer_msg_t * msg, void * ctx)
{
    auto * rawState = static_cast<CallbackState *>(ctx);
    if (msg == nullptr || msg->data == nullptr || msg->size <= 0 || rawState == nullptr ||
        rawState->closed.load(std::memory_order_acquire))
    {
        return 0;
    }

    auto sharedState = rawState->weakSelf.lock();
    if (sharedState == nullptr)
    {
        return 0;
    }

    size_t len         = static_cast<size_t>(msg->size);
    const char * bytes = reinterpret_cast<const char *>(msg->data);
    while (len > 0 && bytes[len - 1] == '\0')
    {
        --len;
    }
    if (len == 0)
    {
        return 0;
    }

    auto * evt = new (std::nothrow) PendingPeerEvent();
    if (evt == nullptr)
    {
        return 0;
    }
    evt->state   = std::move(sharedState);
    evt->kind    = PendingPeerEvent::Kind::kMsg;
    evt->msgType = msg->type;
    evt->payload.assign(bytes, len);
    SchedulePendingEvent(evt);
    return 0;
}

Esp32PeerConnectionFactory::Esp32PeerConnectionFactory(Esp32MediaControl & mediaControl) : mMediaControl(mediaControl) {}

std::unique_ptr<webrtc::PeerConnection> Esp32PeerConnectionFactory::Create(const SessionConfig & config,
                                                                           webrtc::PeerConnection::Events & events)
{
    if (!mCertPreGenerated)
    {
        int err = esp_peer_pre_generate_cert();
        if (err == ESP_PEER_ERR_NONE)
        {
            mCertPreGenerated = true;
        }
        else
        {
            ChipLogError(Camera, "Esp32PeerConnectionFactory: esp_peer_pre_generate_cert failed: %d", err);
        }
    }

    return std::make_unique<Esp32PeerConnection>(config, events, mMediaControl);
}

} // namespace chip::app
