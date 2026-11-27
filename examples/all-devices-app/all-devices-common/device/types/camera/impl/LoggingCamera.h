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

#include <device/types/camera/Camera.h>

#include <cstdint>
#include <vector>

namespace chip {
namespace app {

/**
 * @brief Logging implementation of Camera with in-memory AV stream allocation,
 *        dummy JPEG snapshot capture, and a clean stub WebRTCTransportProvider delegate.
 */
class LoggingCamera : private Clusters::CameraAvStreamManagement::CameraAVStreamManagementDelegate,
                      private Clusters::WebRTCTransportProvider::Delegate,
                      public Camera
{
public:
    struct Context
    {
        FabricTable & fabricTable;
    };

    static Config DefaultConfig();

    explicit LoggingCamera(const Context & context, const Config & config = DefaultConfig());
    ~LoggingCamera() override = default;

private:
    // CameraAVStreamManagementDelegate
    Protocols::InteractionModel::Status
    VideoStreamAllocate(const Clusters::CameraAvStreamManagement::VideoStreamStruct & allocateArgs,
                        uint16_t & outStreamID) override;
    void OnVideoStreamAllocated(const Clusters::CameraAvStreamManagement::VideoStreamStruct & allocatedStream,
                                Clusters::CameraAvStreamManagement::StreamAllocationAction action) override;
    Protocols::InteractionModel::Status VideoStreamModify(uint16_t streamID, Optional<bool> waterMarkEnabled,
                                                          Optional<bool> osdEnabled) override;
    Protocols::InteractionModel::Status VideoStreamDeallocate(uint16_t streamID) override;
    Protocols::InteractionModel::Status
    AudioStreamAllocate(const Clusters::CameraAvStreamManagement::AudioStreamStruct & allocateArgs,
                        uint16_t & outStreamID) override;
    Protocols::InteractionModel::Status AudioStreamDeallocate(uint16_t streamID) override;
    Protocols::InteractionModel::Status SnapshotStreamAllocate(const SnapshotStreamAllocateArgs & allocateArgs,
                                                               uint16_t & outStreamID) override;
    Protocols::InteractionModel::Status SnapshotStreamModify(uint16_t streamID, Optional<bool> waterMarkEnabled,
                                                             Optional<bool> osdEnabled) override;
    Protocols::InteractionModel::Status SnapshotStreamDeallocate(uint16_t streamID) override;
    void OnStreamUsagePrioritiesChanged() override;
    void OnAttributeChanged(AttributeId attributeId) override;
    Protocols::InteractionModel::Status
    CaptureSnapshot(const DataModel::Nullable<uint16_t> streamID,
                    const Clusters::CameraAvStreamManagement::VideoResolutionStruct & resolution,
                    Clusters::CameraAvStreamManagement::ImageSnapshot & outImageSnapshot) override;
    CHIP_ERROR PersistentAttributesLoadedCallback() override;
    const std::vector<Clusters::CameraAvStreamManagement::VideoStreamStruct> & GetAllocatedVideoStreams() const override;
    const std::vector<Clusters::CameraAvStreamManagement::AudioStreamStruct> & GetAllocatedAudioStreams() const override;

    // WebRTCTransportProvider::Delegate (stub until WebRTC transport is wired)
    CHIP_ERROR HandleSolicitOffer(const OfferRequestArgs & args,
                                  Clusters::WebRTCTransportProvider::WebRTCSessionStruct & outSession,
                                  bool & outDeferredOffer) override;
    CHIP_ERROR HandleProvideOffer(const ProvideOfferRequestArgs & args,
                                  Clusters::WebRTCTransportProvider::WebRTCSessionStruct & outSession) override;
    CHIP_ERROR HandleProvideAnswer(uint16_t sessionId, const std::string & sdpAnswer) override;
    CHIP_ERROR
    HandleProvideICECandidates(uint16_t sessionId,
                               const std::vector<Clusters::WebRTCTransportProvider::ICECandidateStruct> & candidates) override;
    CHIP_ERROR HandleEndSession(uint16_t sessionId, Clusters::WebRTCTransportProvider::WebRTCEndReasonEnum reasonCode) override;
    CHIP_ERROR ValidateStreamUsage(Clusters::Globals::StreamUsageEnum streamUsage, Optional<std::vector<uint16_t>> & videoStreams,
                                   Optional<std::vector<uint16_t>> & audioStreams) override;
    CHIP_ERROR ValidateVideoStreamID(uint16_t videoStreamId) override;
    CHIP_ERROR ValidateAudioStreamID(uint16_t audioStreamId) override;
    CHIP_ERROR ValidateVideoStreams(const std::vector<uint16_t> & videoStreams) override;
    CHIP_ERROR ValidateAudioStreams(const std::vector<uint16_t> & audioStreams) override;
    CHIP_ERROR IsStreamUsageSupported(Clusters::Globals::StreamUsageEnum streamUsage) override;
    CHIP_ERROR IsHardPrivacyModeActive(bool & isActive) override;
    CHIP_ERROR IsSoftRecordingPrivacyModeActive(bool & isActive) override;
    CHIP_ERROR IsSoftLivestreamPrivacyModeActive(bool & isActive) override;
    bool HasAllocatedVideoStreams() override;
    bool HasAllocatedAudioStreams() override;
    CHIP_ERROR ValidateSFrameConfig(uint16_t cipherSuite, size_t baseKeyLength) override;
    CHIP_ERROR IsUTCTimeNull(bool & isNull) override;

    uint16_t mNextVideoStreamId    = 1;
    uint16_t mNextAudioStreamId    = 1;
    uint16_t mNextSnapshotStreamId = 1;
};

} // namespace app
} // namespace chip
