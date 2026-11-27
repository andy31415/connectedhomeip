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

#include "sdkconfig.h"

#include <app/clusters/camera-av-stream-management-server/CameraAVStreamManagementCluster.h>
#include <app/clusters/camera-av-stream-management-server/MediaControl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lib/core/CHIPError.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace chip::app {

/**
 * @brief ESP32-S3 camera capture and software H.264 / JPEG media controller.
 *
 * Captures 320x240 YUYV frames from the onboard GC0308 sensor via V4L2,
 * encodes Annex-B H.264 video for active WebRTC sinks on a dedicated PSRAM-backed
 * FreeRTOS task, and encodes JPEG frames on demand for CaptureSnapshot.
 */
class Esp32MediaControl : public Clusters::CameraAvStreamManagement::MediaControl
{
public:
    static constexpr uint16_t kVideoWidth    = 320;
    static constexpr uint16_t kVideoHeight   = 240;
    static constexpr size_t kYuyvFrameBytes  = static_cast<size_t>(kVideoWidth) * kVideoHeight * 2;
    static constexpr uint16_t kSensorFps     = 20;
    static constexpr uint16_t kTargetFps     = CONFIG_ALL_DEVICES_CAMERA_FPS;
    static constexpr uint32_t kTargetBitrate = CONFIG_ALL_DEVICES_CAMERA_BITRATE_BPS;

    class VideoFrameSink
    {
    public:
        virtual ~VideoFrameSink()                                                                     = default;
        virtual void OnVideoFrame(uint32_t ptsMs, const uint8_t * data, size_t size, bool isKeyframe) = 0;
    };

    Esp32MediaControl() = default;
    ~Esp32MediaControl() override;

    void SetAvsmCluster(Clusters::CameraAvStreamManagement::CameraAVStreamManagementCluster * cluster);

    void AddVideoSink(VideoFrameSink * sink);
    void RemoveVideoSink(VideoFrameSink * sink);
    void RequestKeyframe();

    // MediaControl interface (called on the Matter thread)
    CHIP_ERROR StartVideoStream(const Clusters::CameraAvStreamManagement::VideoStreamStruct & stream) override;
    CHIP_ERROR StopVideoStream(uint16_t streamId) override;
    CHIP_ERROR StartAudioStream(const Clusters::CameraAvStreamManagement::AudioStreamStruct & stream) override;
    CHIP_ERROR StopAudioStream(uint16_t streamId) override;
    CHIP_ERROR StartSnapshotStream(const Clusters::CameraAvStreamManagement::SnapshotStreamStruct & stream) override;
    CHIP_ERROR StopSnapshotStream(uint16_t streamId) override;
    CHIP_ERROR CaptureSnapshot(std::optional<uint16_t> streamId,
                               const Clusters::CameraAvStreamManagement::VideoResolutionStruct & resolution,
                               Clusters::CameraAvStreamManagement::ImageSnapshot & outSnapshot) override;
    void OnAttributeChanged(AttributeId attributeId) override;

private:
    CHIP_ERROR EnsureCaptureStartedLocked();
    void StopCaptureTaskLocked();
    CHIP_ERROR CaptureSingleYuyvFrame(uint8_t * destBuf, size_t destSize);
    CHIP_ERROR EnsureCameraBspStartedLocked();
    void RefreshPrivacyState();
    void DispatchVideoFrame(uint32_t ptsMs, const uint8_t * data, size_t size, bool isKeyframe);

    static void CaptureTaskEntry(void * arg);
    void RunCaptureLoop();

    Clusters::CameraAvStreamManagement::CameraAVStreamManagementCluster * mAvsmCluster = nullptr;
    std::atomic<bool> mPrivacyBlocked{ false };
    std::atomic<bool> mKeyframeRequested{ true };

    std::mutex mStreamMutex;
    std::vector<uint16_t> mActiveVideoStreams;
    std::vector<uint16_t> mActiveSnapshotStreams;

    // Task lifecycle for video capture + H.264 encode
    std::atomic<bool> mCaptureRunning{ false };
    TaskHandle_t mCaptureTaskHandle   = nullptr;
    StackType_t * mCaptureTaskStack   = nullptr;
    StaticTask_t * mCaptureTaskTcb    = nullptr;
    SemaphoreHandle_t mCaptureDoneSem = nullptr;

    // Serializes BSP/V4L2 access between video task teardown and on-demand snapshot capture.
    std::mutex mCameraMutex;
    bool mCameraBspStarted = false;

    // Latest YUYV frame cached in PSRAM while the video task is running so CaptureSnapshot
    // does not contend for /dev/video2.
    std::mutex mFrameCacheMutex;
    uint8_t * mLatestYuyvFrame = nullptr;
    bool mLatestYuyvValid      = false;

    // Protected sinks list. Lock order: mSinksMutex must always be acquired before any sink lock.
    std::mutex mSinksMutex;
    std::vector<VideoFrameSink *> mSinks;
};

} // namespace chip::app
