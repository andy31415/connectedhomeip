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

#include "Esp32MediaControl.h"

#include <clusters/CameraAvStreamManagement/AttributeIds.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

#if CONFIG_DEVICE_TYPE_M5STACK_CORES3
#include "bsp/m5stack_core_s3.h"
#endif

extern "C" {
#include "esp_h264_alloc.h"
}
#include "esp_h264_enc_param.h"
#include "esp_h264_enc_single.h"
#include "esp_h264_enc_single_sw.h"
#include "esp_h264_types.h"
#include "esp_heap_caps.h"
#include "esp_jpeg_common.h"
#include "esp_jpeg_enc.h"
#include "esp_timer.h"
#include "esp_video_device.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace chip::app {

using namespace chip::app::Clusters::CameraAvStreamManagement;

namespace {

constexpr size_t kV4l2BufferCount      = 2;
constexpr uint32_t kVideoTaskStackSize = 20480;
constexpr UBaseType_t kVideoTaskPrio   = 3;
constexpr BaseType_t kVideoTaskCore    = 1;
constexpr uint8_t kFallbackJpegQuality = 30;

struct V4l2DeviceSession
{
    int fd = -1;
    std::vector<void *> buffers;
    std::vector<size_t> lengths;
    bool streaming = false;

    ~V4l2DeviceSession() { Close(); }

    void Close()
    {
        if (streaming && fd >= 0)
        {
            int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ioctl(fd, VIDIOC_STREAMOFF, &type);
            streaming = false;
        }
        for (size_t i = 0; i < buffers.size(); ++i)
        {
            if (buffers[i] != nullptr && buffers[i] != MAP_FAILED)
            {
                munmap(buffers[i], lengths[i]);
            }
        }
        buffers.clear();
        lengths.clear();
        if (fd >= 0)
        {
            close(fd);
            fd = -1;
        }
    }
};

CHIP_ERROR OpenV4l2YuyvDevice(V4l2DeviceSession & session, const char * devicePath)
{
    session.Close();

    int fd = open(devicePath, O_RDONLY);
    if (fd < 0)
    {
        ChipLogError(Camera, "Esp32MediaControl: open(%s) failed: errno=%d", devicePath, errno);
        return CHIP_ERROR_INTERNAL;
    }
    session.fd = fd;

    struct v4l2_format fmt  = {};
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = Esp32MediaControl::kVideoWidth;
    fmt.fmt.pix.height      = Esp32MediaControl::kVideoHeight;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    if (ioctl(fd, VIDIOC_S_FMT, &fmt) != 0)
    {
        ChipLogError(Camera, "Esp32MediaControl: VIDIOC_S_FMT failed: errno=%d", errno);
        session.Close();
        return CHIP_ERROR_INTERNAL;
    }

    struct v4l2_requestbuffers req = {};
    req.count                      = kV4l2BufferCount;
    req.type                       = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory                     = V4L2_MEMORY_MMAP;
    if (ioctl(fd, VIDIOC_REQBUFS, &req) != 0 || req.count == 0)
    {
        ChipLogError(Camera, "Esp32MediaControl: VIDIOC_REQBUFS failed: errno=%d", errno);
        session.Close();
        return CHIP_ERROR_INTERNAL;
    }

    session.buffers.resize(req.count, nullptr);
    session.lengths.resize(req.count, 0);

    for (uint32_t i = 0; i < req.count; ++i)
    {
        struct v4l2_buffer buf = {};
        buf.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory             = V4L2_MEMORY_MMAP;
        buf.index              = i;
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) != 0)
        {
            ChipLogError(Camera, "Esp32MediaControl: VIDIOC_QUERYBUF(%lu) failed: errno=%d", static_cast<unsigned long>(i), errno);
            session.Close();
            return CHIP_ERROR_INTERNAL;
        }

        void * mapped = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        if (mapped == MAP_FAILED)
        {
            ChipLogError(Camera, "Esp32MediaControl: mmap(%lu) failed: errno=%d", static_cast<unsigned long>(i), errno);
            session.Close();
            return CHIP_ERROR_NO_MEMORY;
        }
        session.buffers[i] = mapped;
        session.lengths[i] = buf.length;

        if (ioctl(fd, VIDIOC_QBUF, &buf) != 0)
        {
            ChipLogError(Camera, "Esp32MediaControl: VIDIOC_QBUF(%lu) failed: errno=%d", static_cast<unsigned long>(i), errno);
            session.Close();
            return CHIP_ERROR_INTERNAL;
        }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0)
    {
        ChipLogError(Camera, "Esp32MediaControl: VIDIOC_STREAMON failed: errno=%d", errno);
        session.Close();
        return CHIP_ERROR_INTERNAL;
    }
    session.streaming = true;
    return CHIP_NO_ERROR;
}

const char * GetCameraDevicePath()
{
#if CONFIG_DEVICE_TYPE_M5STACK_CORES3
    return BSP_CAMERA_DEVICE;
#else
    return ESP_VIDEO_DVP_DEVICE_NAME;
#endif
}

} // namespace

Esp32MediaControl::~Esp32MediaControl()
{
    std::lock_guard<std::mutex> lock(mStreamMutex);
    mActiveVideoStreams.clear();
    mActiveSnapshotStreams.clear();
    StopCaptureTaskLocked();

    std::lock_guard<std::mutex> cacheLock(mFrameCacheMutex);
    if (mLatestYuyvFrame != nullptr)
    {
        heap_caps_free(mLatestYuyvFrame);
        mLatestYuyvFrame = nullptr;
        mLatestYuyvValid = false;
    }
}

void Esp32MediaControl::SetAvsmCluster(CameraAVStreamManagementCluster * cluster)
{
    mAvsmCluster = cluster;
    RefreshPrivacyState();
}

void Esp32MediaControl::RefreshPrivacyState()
{
    if (mAvsmCluster == nullptr)
    {
        mPrivacyBlocked.store(false, std::memory_order_release);
        return;
    }

    bool blocked = mAvsmCluster->GetHardPrivacyModeOn() || mAvsmCluster->GetSoftLivestreamPrivacyModeEnabled();
    bool prev    = mPrivacyBlocked.exchange(blocked, std::memory_order_acq_rel);
    if (prev && !blocked)
    {
        RequestKeyframe();
    }
}

void Esp32MediaControl::AddVideoSink(VideoFrameSink * sink)
{
    VerifyOrReturn(sink != nullptr);
    std::lock_guard<std::mutex> lock(mSinksMutex);
    if (std::find(mSinks.begin(), mSinks.end(), sink) == mSinks.end())
    {
        mSinks.push_back(sink);
    }
    RequestKeyframe();
}

void Esp32MediaControl::RemoveVideoSink(VideoFrameSink * sink)
{
    VerifyOrReturn(sink != nullptr);
    std::lock_guard<std::mutex> lock(mSinksMutex);
    mSinks.erase(std::remove(mSinks.begin(), mSinks.end(), sink), mSinks.end());
}

void Esp32MediaControl::RequestKeyframe()
{
    mKeyframeRequested.store(true, std::memory_order_release);
}

void Esp32MediaControl::DispatchVideoFrame(uint32_t ptsMs, const uint8_t * data, size_t size, bool isKeyframe)
{
    std::lock_guard<std::mutex> lock(mSinksMutex);
    for (VideoFrameSink * sink : mSinks)
    {
        sink->OnVideoFrame(ptsMs, data, size, isKeyframe);
    }
}

CHIP_ERROR Esp32MediaControl::EnsureCameraBspStartedLocked()
{
    if (mCameraBspStarted)
    {
        return CHIP_NO_ERROR;
    }

#if CONFIG_DEVICE_TYPE_M5STACK_CORES3
    bsp_camera_cfg_t camCfg = {};
    esp_err_t err           = bsp_camera_start(&camCfg);
    if (err != ESP_OK)
    {
        ChipLogError(Camera, "Esp32MediaControl: bsp_camera_start failed: %d", static_cast<int>(err));
        return CHIP_ERROR_INTERNAL;
    }
    mCameraBspStarted = true;
    return CHIP_NO_ERROR;
#else
    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
#endif
}

CHIP_ERROR Esp32MediaControl::StartVideoStream(const VideoStreamStruct & stream)
{
    std::lock_guard<std::mutex> lock(mStreamMutex);

    if (std::find(mActiveVideoStreams.begin(), mActiveVideoStreams.end(), stream.videoStreamID) == mActiveVideoStreams.end())
    {
        mActiveVideoStreams.push_back(stream.videoStreamID);
    }

    CHIP_ERROR err = EnsureCaptureStartedLocked();
    if (err != CHIP_NO_ERROR)
    {
        mActiveVideoStreams.erase(std::remove(mActiveVideoStreams.begin(), mActiveVideoStreams.end(), stream.videoStreamID),
                                  mActiveVideoStreams.end());
        return err;
    }

    RequestKeyframe();
    ChipLogProgress(Camera, "Esp32MediaControl: Started video stream %u", stream.videoStreamID);
    return CHIP_NO_ERROR;
}

CHIP_ERROR Esp32MediaControl::StopVideoStream(uint16_t streamId)
{
    std::lock_guard<std::mutex> lock(mStreamMutex);

    mActiveVideoStreams.erase(std::remove(mActiveVideoStreams.begin(), mActiveVideoStreams.end(), streamId),
                              mActiveVideoStreams.end());
    if (mActiveVideoStreams.empty())
    {
        StopCaptureTaskLocked();
    }

    ChipLogProgress(Camera, "Esp32MediaControl: Stopped video stream %u", streamId);
    return CHIP_NO_ERROR;
}

CHIP_ERROR Esp32MediaControl::StartAudioStream(const AudioStreamStruct & stream)
{
    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}

CHIP_ERROR Esp32MediaControl::StopAudioStream(uint16_t streamId)
{
    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}

CHIP_ERROR Esp32MediaControl::StartSnapshotStream(const SnapshotStreamStruct & stream)
{
    std::lock_guard<std::mutex> lock(mStreamMutex);
    if (std::find(mActiveSnapshotStreams.begin(), mActiveSnapshotStreams.end(), stream.snapshotStreamID) ==
        mActiveSnapshotStreams.end())
    {
        mActiveSnapshotStreams.push_back(stream.snapshotStreamID);
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR Esp32MediaControl::StopSnapshotStream(uint16_t streamId)
{
    std::lock_guard<std::mutex> lock(mStreamMutex);
    mActiveSnapshotStreams.erase(std::remove(mActiveSnapshotStreams.begin(), mActiveSnapshotStreams.end(), streamId),
                                 mActiveSnapshotStreams.end());
    return CHIP_NO_ERROR;
}

void Esp32MediaControl::OnAttributeChanged(AttributeId attributeId)
{
    if (attributeId == Attributes::HardPrivacyModeOn::Id || attributeId == Attributes::SoftLivestreamPrivacyModeEnabled::Id)
    {
        RefreshPrivacyState();
    }
}

CHIP_ERROR Esp32MediaControl::EnsureCaptureStartedLocked()
{
    if (mCaptureTaskHandle != nullptr)
    {
        return CHIP_NO_ERROR;
    }

    {
        std::lock_guard<std::mutex> camLock(mCameraMutex);
        ReturnErrorOnFailure(EnsureCameraBspStartedLocked());
    }

    {
        std::lock_guard<std::mutex> cacheLock(mFrameCacheMutex);
        if (mLatestYuyvFrame == nullptr)
        {
            mLatestYuyvFrame = static_cast<uint8_t *>(heap_caps_malloc(kYuyvFrameBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            VerifyOrReturnError(mLatestYuyvFrame != nullptr, CHIP_ERROR_NO_MEMORY);
        }
        mLatestYuyvValid = false;
    }

    mCaptureDoneSem = xSemaphoreCreateBinary();
    VerifyOrReturnError(mCaptureDoneSem != nullptr, CHIP_ERROR_NO_MEMORY);

    mCaptureTaskStack = static_cast<StackType_t *>(heap_caps_malloc(kVideoTaskStackSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    mCaptureTaskTcb   = static_cast<StaticTask_t *>(heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (mCaptureTaskStack == nullptr || mCaptureTaskTcb == nullptr)
    {
        StopCaptureTaskLocked();
        return CHIP_ERROR_NO_MEMORY;
    }

    mCaptureRunning.store(true, std::memory_order_release);
    mCaptureTaskHandle = xTaskCreateStaticPinnedToCore(CaptureTaskEntry, "cam_h264", kVideoTaskStackSize / sizeof(StackType_t),
                                                       this, kVideoTaskPrio, mCaptureTaskStack, mCaptureTaskTcb, kVideoTaskCore);
    if (mCaptureTaskHandle == nullptr)
    {
        mCaptureRunning.store(false, std::memory_order_release);
        StopCaptureTaskLocked();
        return CHIP_ERROR_NO_MEMORY;
    }

    return CHIP_NO_ERROR;
}

void Esp32MediaControl::StopCaptureTaskLocked()
{
    if (mCaptureTaskHandle != nullptr)
    {
        mCaptureRunning.store(false, std::memory_order_release);
        if (mCaptureDoneSem != nullptr)
        {
            xSemaphoreTake(mCaptureDoneSem, portMAX_DELAY);
        }
        mCaptureTaskHandle = nullptr;
    }

    if (mCaptureDoneSem != nullptr)
    {
        vSemaphoreDelete(mCaptureDoneSem);
        mCaptureDoneSem = nullptr;
    }
    if (mCaptureTaskStack != nullptr)
    {
        heap_caps_free(mCaptureTaskStack);
        mCaptureTaskStack = nullptr;
    }
    if (mCaptureTaskTcb != nullptr)
    {
        heap_caps_free(mCaptureTaskTcb);
        mCaptureTaskTcb = nullptr;
    }

    std::lock_guard<std::mutex> cacheLock(mFrameCacheMutex);
    mLatestYuyvValid = false;
}

void Esp32MediaControl::CaptureTaskEntry(void * arg)
{
    static_cast<Esp32MediaControl *>(arg)->RunCaptureLoop();
}

void Esp32MediaControl::RunCaptureLoop()
{
    V4l2DeviceSession v4l2;
    esp_h264_enc_handle_t encoder      = nullptr;
    esp_h264_enc_param_sw_handle_t prm = nullptr;
    uint8_t * rawYuyvBuf               = nullptr;
    uint8_t * encodedBuf               = nullptr;
    uint32_t rawYuyvBufSize            = 0;
    uint32_t encodedBufSize            = 0;

    auto cleanup = [&]() {
        if (encoder != nullptr)
        {
            esp_h264_enc_close(encoder);
            esp_h264_enc_del(encoder);
            encoder = nullptr;
        }
        if (encodedBuf != nullptr)
        {
            esp_h264_free(encodedBuf);
            encodedBuf = nullptr;
        }
        if (rawYuyvBuf != nullptr)
        {
            esp_h264_free(rawYuyvBuf);
            rawYuyvBuf = nullptr;
        }
        v4l2.Close();
    };

    rawYuyvBuf = static_cast<uint8_t *>(esp_h264_aligned_malloc(16, 1, kYuyvFrameBytes, &rawYuyvBufSize, ESP_H264_MEM_SPIRAM));
    encodedBuf = static_cast<uint8_t *>(esp_h264_aligned_malloc(16, 1, kYuyvFrameBytes, &encodedBufSize, ESP_H264_MEM_SPIRAM));
    if (rawYuyvBuf == nullptr || encodedBuf == nullptr)
    {
        ChipLogError(Camera, "Esp32MediaControl: Failed to allocate PSRAM video buffers");
        cleanup();
        mCaptureRunning.store(false, std::memory_order_release);
        xSemaphoreGive(mCaptureDoneSem);
        vTaskDelete(nullptr);
        return;
    }

    const uint8_t targetFps  = static_cast<uint8_t>(std::max<int>(1, kTargetFps));
    const uint8_t normalGop  = static_cast<uint8_t>(std::min<int>(250, std::max<int>(2, targetFps * 2)));
    const int64_t intervalUs = 1000000LL / targetFps;

    esp_h264_enc_cfg_sw_t encCfg = {};
    encCfg.pic_type              = ESP_H264_RAW_FMT_YUYV;
    encCfg.gop                   = normalGop;
    encCfg.fps                   = targetFps;
    encCfg.res.width             = kVideoWidth;
    encCfg.res.height            = kVideoHeight;
    encCfg.rc.bitrate            = kTargetBitrate;
    encCfg.rc.qp_min             = 20;
    encCfg.rc.qp_max             = 40;

    if (esp_h264_enc_sw_new(&encCfg, &encoder) != ESP_H264_ERR_OK ||
        esp_h264_enc_sw_get_param_hd(encoder, &prm) != ESP_H264_ERR_OK || esp_h264_enc_open(encoder) != ESP_H264_ERR_OK)
    {
        ChipLogError(Camera, "Esp32MediaControl: Failed to open H.264 SW encoder");
        cleanup();
        mCaptureRunning.store(false, std::memory_order_release);
        xSemaphoreGive(mCaptureDoneSem);
        vTaskDelete(nullptr);
        return;
    }

    {
        std::lock_guard<std::mutex> camLock(mCameraMutex);
        if (OpenV4l2YuyvDevice(v4l2, GetCameraDevicePath()) != CHIP_NO_ERROR)
        {
            cleanup();
            mCaptureRunning.store(false, std::memory_order_release);
            xSemaphoreGive(mCaptureDoneSem);
            vTaskDelete(nullptr);
            return;
        }
    }

    int64_t nextEncodeUs = esp_timer_get_time();
    bool forcedIdrGop    = false;

    while (mCaptureRunning.load(std::memory_order_acquire))
    {
        struct v4l2_buffer buf = {};
        buf.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory             = V4L2_MEMORY_MMAP;
        if (ioctl(v4l2.fd, VIDIOC_DQBUF, &buf) != 0)
        {
            vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(10)));
            continue;
        }

        const bool frameOk      = (buf.flags & V4L2_BUF_FLAG_DONE) != 0 && buf.bytesused >= kYuyvFrameBytes;
        const int64_t nowUs     = esp_timer_get_time();
        const bool shouldEncode = frameOk && (nowUs >= nextEncodeUs);

        if (shouldEncode)
        {
            memcpy(rawYuyvBuf, v4l2.buffers[buf.index], kYuyvFrameBytes);
        }

        // Return the MMAP buffer immediately so the DVP DMA does not stall during SW encode.
        ioctl(v4l2.fd, VIDIOC_QBUF, &buf);

        if (!shouldEncode)
        {
            continue;
        }

        nextEncodeUs = std::max(nextEncodeUs + intervalUs, nowUs);

        if (mPrivacyBlocked.load(std::memory_order_acquire))
        {
            continue;
        }

        {
            std::lock_guard<std::mutex> cacheLock(mFrameCacheMutex);
            if (mLatestYuyvFrame != nullptr)
            {
                memcpy(mLatestYuyvFrame, rawYuyvBuf, kYuyvFrameBytes);
                mLatestYuyvValid = true;
            }
        }

        {
            std::lock_guard<std::mutex> sinksLock(mSinksMutex);
            if (mSinks.empty())
            {
                continue;
            }
        }

        // OpenH264 SW encoder returns ESP_H264_ERR_UNSUPPORTED for esp_h264_enc_force_idr();
        // temporarily setting GOP = 1 forces the next frame to be an IDR with SPS/PPS.
        if (mKeyframeRequested.exchange(false, std::memory_order_acq_rel) && !forcedIdrGop && prm != nullptr)
        {
            if (esp_h264_enc_set_gop(prm, 1) == ESP_H264_ERR_OK)
            {
                forcedIdrGop = true;
            }
        }

        esp_h264_enc_in_frame_t inFrame   = {};
        inFrame.raw_data.buffer           = rawYuyvBuf;
        inFrame.raw_data.len              = kYuyvFrameBytes;
        inFrame.pts                       = static_cast<uint32_t>(nowUs / 1000);
        esp_h264_enc_out_frame_t outFrame = {};
        outFrame.raw_data.buffer          = encodedBuf;
        outFrame.raw_data.len             = encodedBufSize;

        esp_h264_err_t encErr = esp_h264_enc_process(encoder, &inFrame, &outFrame);
        if (encErr == ESP_H264_ERR_OK && outFrame.length > 0)
        {
            const bool isIdr = (outFrame.frame_type == ESP_H264_FRAME_TYPE_IDR);
            if (forcedIdrGop && isIdr && prm != nullptr)
            {
                esp_h264_enc_set_gop(prm, normalGop);
                forcedIdrGop = false;
            }
            DispatchVideoFrame(static_cast<uint32_t>(nowUs / 1000), outFrame.raw_data.buffer, outFrame.length, isIdr);
        }
        else
        {
            ChipLogError(Camera, "Esp32MediaControl: H.264 encode failed: %d", static_cast<int>(encErr));
        }
    }

    {
        std::lock_guard<std::mutex> camLock(mCameraMutex);
        cleanup();
    }

    xSemaphoreGive(mCaptureDoneSem);
    vTaskDelete(nullptr);
}

CHIP_ERROR Esp32MediaControl::CaptureSingleYuyvFrame(uint8_t * destBuf, size_t destSize)
{
    VerifyOrReturnError(destBuf != nullptr && destSize >= kYuyvFrameBytes, CHIP_ERROR_INVALID_ARGUMENT);

    // If the capture loop is actively pumping frames, reuse the latest YUYV frame instead of
    // opening a second handle on /dev/video2.
    {
        std::lock_guard<std::mutex> cacheLock(mFrameCacheMutex);
        if (mCaptureRunning.load(std::memory_order_acquire) && mLatestYuyvValid && mLatestYuyvFrame != nullptr)
        {
            memcpy(destBuf, mLatestYuyvFrame, kYuyvFrameBytes);
            return CHIP_NO_ERROR;
        }
    }

    std::lock_guard<std::mutex> camLock(mCameraMutex);
    ReturnErrorOnFailure(EnsureCameraBspStartedLocked());

    V4l2DeviceSession v4l2;
    ReturnErrorOnFailure(OpenV4l2YuyvDevice(v4l2, GetCameraDevicePath()));

    // Discard the first frame so DMA/sensor synchronization settles, then keep the second.
    bool captured = false;
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        struct v4l2_buffer buf = {};
        buf.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory             = V4L2_MEMORY_MMAP;
        if (ioctl(v4l2.fd, VIDIOC_DQBUF, &buf) != 0)
        {
            break;
        }

        const bool valid = (buf.flags & V4L2_BUF_FLAG_DONE) != 0 && buf.bytesused >= kYuyvFrameBytes;
        if (attempt >= 1 && valid)
        {
            memcpy(destBuf, v4l2.buffers[buf.index], kYuyvFrameBytes);
            captured = true;
        }

        ioctl(v4l2.fd, VIDIOC_QBUF, &buf);
        if (captured)
        {
            break;
        }
    }

    v4l2.Close();
    return captured ? CHIP_NO_ERROR : CHIP_ERROR_INTERNAL;
}

CHIP_ERROR Esp32MediaControl::CaptureSnapshot(std::optional<uint16_t> streamId, const VideoResolutionStruct & resolution,
                                              ImageSnapshot & outSnapshot)
{
    if (mPrivacyBlocked.load(std::memory_order_acquire))
    {
        return CHIP_ERROR_INCORRECT_STATE;
    }

    uint8_t * alignedYuyv = static_cast<uint8_t *>(jpeg_calloc_align(kYuyvFrameBytes, 16));
    VerifyOrReturnError(alignedYuyv != nullptr, CHIP_ERROR_NO_MEMORY);

    constexpr size_t kOutBufSize = std::max<size_t>(static_cast<size_t>(kVideoWidth) * kVideoHeight, 65536);
    uint8_t * jpegOut            = static_cast<uint8_t *>(heap_caps_malloc(kOutBufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (jpegOut == nullptr)
    {
        jpeg_free_align(alignedYuyv);
        return CHIP_ERROR_NO_MEMORY;
    }

    CHIP_ERROR err = CaptureSingleYuyvFrame(alignedYuyv, kYuyvFrameBytes);
    if (err != CHIP_NO_ERROR)
    {
        heap_caps_free(jpegOut);
        jpeg_free_align(alignedYuyv);
        return err;
    }

    jpeg_enc_config_t cfg   = DEFAULT_JPEG_ENC_CONFIG();
    cfg.width               = kVideoWidth;
    cfg.height              = kVideoHeight;
    cfg.src_type            = JPEG_PIXEL_FORMAT_YCbYCr;
    cfg.subsampling         = JPEG_SUBSAMPLE_420;
    cfg.quality             = CONFIG_ALL_DEVICES_CAMERA_JPEG_QUALITY;
    cfg.rotate              = JPEG_ROTATE_0D;
    cfg.task_enable         = false;
    jpeg_enc_handle_t enc   = nullptr;
    int jpegBytes           = 0;
    jpeg_error_t jpegStatus = jpeg_enc_open(&cfg, &enc);
    if (jpegStatus == JPEG_ERR_OK && enc != nullptr)
    {
        jpegStatus = jpeg_enc_process(enc, alignedYuyv, static_cast<int>(kYuyvFrameBytes), jpegOut, static_cast<int>(kOutBufSize),
                                      &jpegBytes);
        if (jpegStatus == JPEG_ERR_OK && static_cast<size_t>(jpegBytes) > kMaxSnapshotImageSize &&
            cfg.quality > kFallbackJpegQuality)
        {
            if (jpeg_enc_set_quality(enc, kFallbackJpegQuality) == JPEG_ERR_OK)
            {
                jpegStatus = jpeg_enc_process(enc, alignedYuyv, static_cast<int>(kYuyvFrameBytes), jpegOut,
                                              static_cast<int>(kOutBufSize), &jpegBytes);
            }
        }
        jpeg_enc_close(enc);
    }

    jpeg_free_align(alignedYuyv);

    if (jpegStatus != JPEG_ERR_OK || jpegBytes <= 0)
    {
        ChipLogError(Camera, "Esp32MediaControl: JPEG encode failed: status=%d bytes=%d", static_cast<int>(jpegStatus), jpegBytes);
        heap_caps_free(jpegOut);
        return CHIP_ERROR_INTERNAL;
    }

    outSnapshot.data.assign(jpegOut, jpegOut + jpegBytes);
    outSnapshot.imageRes   = { kVideoWidth, kVideoHeight };
    outSnapshot.imageCodec = ImageCodecEnum::kJpeg;
    heap_caps_free(jpegOut);

    ChipLogProgress(Camera, "Esp32MediaControl: Captured JPEG snapshot (%d bytes)", jpegBytes);
    return CHIP_NO_ERROR;
}

} // namespace chip::app
