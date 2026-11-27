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
#include "Esp32PeerConnection.h"

#include <app/CASESessionManager.h>
#include <app/clusters/camera-av-stream-management-server/StreamAllocator.h>
#include <app/clusters/webrtc-transport-provider-server/WebRTCProviderSessionDelegate.h>
#include <app/webrtc/WebRTCTransportRequestorClient.h>
#include <credentials/FabricTable.h>
#include <device/types/camera/Camera.h>
#include <lib/support/TimerDelegate.h>

#include <memory>

namespace chip::app {

/**
 * @brief M5Stack CoreS3 Matter Camera device combining AVSM StreamAllocator,
 *        WebRTCProviderSessionDelegate, Esp32MediaControl, and Esp32PeerConnectionFactory.
 */
class Esp32Camera : public Camera, private Clusters::WebRTCTransportProvider::Delegate
{
public:
    struct Context
    {
        FabricTable & fabricTable;
        CASESessionManager & caseSessionManager;
        TimerDelegate & timerDelegate;
    };

    explicit Esp32Camera(const Context & context);
    ~Esp32Camera() override;

protected:
    CHIP_ERROR RegisterAdditionalClusters(EndpointId endpoint, CodeDrivenDataModelProvider & provider) override;
    void UnregisterAdditionalClusters(CodeDrivenDataModelProvider & provider) override;

private:
    // WebRTCTransportProvider::Delegate forwarding to mSessionDelegate
    CHIP_ERROR HandleSolicitOffer(const OfferRequestArgs & args,
                                  Clusters::WebRTCTransportProvider::WebRTCSessionStruct & outSession,
                                  bool & outDeferredOffer) override;
    CHIP_ERROR HandleProvideOffer(const ProvideOfferRequestArgs & args,
                                  Clusters::WebRTCTransportProvider::WebRTCSessionStruct & outSession) override;
    CHIP_ERROR HandleProvideAnswer(uint16_t sessionId, const std::string & sdp) override;
    CHIP_ERROR
    HandleProvideICECandidates(uint16_t sessionId,
                               const std::vector<Clusters::WebRTCTransportProvider::ICECandidateStruct> & candidates) override;
    CHIP_ERROR HandleEndSession(uint16_t sessionId, Clusters::WebRTCTransportProvider::WebRTCEndReasonEnum reasonCode) override;
    CHIP_ERROR ValidateStreamUsage(Clusters::WebRTCTransportProvider::StreamUsageEnum streamUsage,
                                   Optional<std::vector<uint16_t>> & videoStreams,
                                   Optional<std::vector<uint16_t>> & audioStreams) override;
    CHIP_ERROR ValidateVideoStreamID(uint16_t videoStreamId) override;
    CHIP_ERROR ValidateAudioStreamID(uint16_t audioStreamId) override;
    CHIP_ERROR ValidateVideoStreams(const std::vector<uint16_t> & videoStreams) override;
    CHIP_ERROR ValidateAudioStreams(const std::vector<uint16_t> & audioStreams) override;
    CHIP_ERROR IsStreamUsageSupported(Clusters::WebRTCTransportProvider::StreamUsageEnum streamUsage) override;
    CHIP_ERROR IsHardPrivacyModeActive(bool & isActive) override;
    CHIP_ERROR IsSoftRecordingPrivacyModeActive(bool & isActive) override;
    CHIP_ERROR IsSoftLivestreamPrivacyModeActive(bool & isActive) override;
    bool HasAllocatedVideoStreams() override;
    bool HasAllocatedAudioStreams() override;
    CHIP_ERROR ValidateSFrameConfig(uint16_t cipherSuite, size_t kidSize) override;
    CHIP_ERROR IsUTCTimeNull(bool & isNull) override;

    static Camera::Config DefaultConfig();
    static Clusters::CameraAvStreamManagement::StreamAllocator::Config DefaultAllocatorConfig();

    TimerDelegate & mTimerDelegate;
    Esp32MediaControl mMediaControl;
    Clusters::CameraAvStreamManagement::StreamAllocator mStreamAllocator;
    Esp32PeerConnectionFactory mPeerFactory;
    webrtc::WebRTCTransportRequestorClient mRequestorClient;
    std::unique_ptr<Clusters::WebRTCTransportProvider::WebRTCProviderSessionDelegate> mSessionDelegate;
};

} // namespace chip::app
