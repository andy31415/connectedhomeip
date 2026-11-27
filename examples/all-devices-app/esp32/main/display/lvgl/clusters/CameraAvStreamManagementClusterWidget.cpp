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

#include "CameraAvStreamManagementClusterWidget.h"
#include "DisplayNotificationHub.h"
#include "Ui.h"

#include <clusters/CameraAvStreamManagement/AttributeIds.h>
#include <clusters/CameraAvStreamManagement/Ids.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/StringBuilder.h>
#include <platform/CHIPDeviceLayer.h>

namespace chip::app {

namespace {

using namespace Clusters::CameraAvStreamManagement;

struct AvsmWidgetRefs
{
    CameraAVStreamManagementCluster * cluster;
    lv_obj_t * streamsLabel;
    lv_obj_t * detailsLabel;
    lv_obj_t * privacyLabel;
    lv_obj_t * privacyBtn;
    lv_obj_t * privacyBtnLabel;
};

void UpdateAvsmWidget(const AvsmWidgetRefs & refs)
{
    auto & cluster      = *refs.cluster;
    const auto & videos = cluster.GetAllocatedVideoStreams();
    const auto & audios = cluster.GetAllocatedAudioStreams();
    const auto & snaps  = cluster.GetAllocatedSnapshotStreams();

    StringBuilder<96> summary;
    summary.AddFormat("Video: %u  Snap: %u  Audio: %u", static_cast<unsigned int>(videos.size()),
                      static_cast<unsigned int>(snaps.size()), static_cast<unsigned int>(audios.size()));
    lv_label_set_text(refs.streamsLabel, summary.c_str());

    StringBuilder<160> details;
    if (videos.empty() && snaps.empty())
    {
        const auto & sensor = cluster.GetVideoSensorParams();
        details.AddFormat(
            "Sensor %ux%u @ %u fps (no active streams)", static_cast<unsigned int>(sensor.sensorWidth),
            static_cast<unsigned int>(sensor.sensorHeight),
            static_cast<unsigned int>(cluster.GetCurrentFrameRate() > 0 ? cluster.GetCurrentFrameRate() : sensor.maxFPS));
    }
    else
    {
        for (const auto & v : videos)
        {
            details.AddFormat("%sV#%u %ux%u @%ufps ref=%u", details.Empty() ? "" : "\n", static_cast<unsigned int>(v.videoStreamID),
                              static_cast<unsigned int>(v.maxResolution.width), static_cast<unsigned int>(v.maxResolution.height),
                              static_cast<unsigned int>(v.maxFrameRate), static_cast<unsigned int>(v.referenceCount));
        }
        for (const auto & s : snaps)
        {
            details.AddFormat("%sS#%u %ux%u ref=%u", details.Empty() ? "" : "\n", static_cast<unsigned int>(s.snapshotStreamID),
                              static_cast<unsigned int>(s.maxResolution.width), static_cast<unsigned int>(s.maxResolution.height),
                              static_cast<unsigned int>(s.referenceCount));
        }
    }
    lv_label_set_text(refs.detailsLabel, details.c_str());

    if (refs.privacyLabel != nullptr && refs.privacyBtn != nullptr && refs.privacyBtnLabel != nullptr)
    {
        const bool softPrivacy = cluster.GetSoftLivestreamPrivacyModeEnabled();
        const bool hardPrivacy = cluster.GetHardPrivacyModeOn();
        if (hardPrivacy)
        {
            lv_label_set_text_static(refs.privacyLabel, "Privacy: HARD ON");
            lv_obj_set_style_text_color(refs.privacyLabel, Ui::Danger(), LV_PART_MAIN);
        }
        else if (softPrivacy)
        {
            lv_label_set_text_static(refs.privacyLabel, "Privacy: ON");
            lv_obj_set_style_text_color(refs.privacyLabel, Ui::Warning(), LV_PART_MAIN);
        }
        else
        {
            lv_label_set_text_static(refs.privacyLabel, "Privacy: OFF");
            lv_obj_set_style_text_color(refs.privacyLabel, Ui::Success(), LV_PART_MAIN);
        }

        lv_label_set_text_static(refs.privacyBtnLabel, softPrivacy ? "Resume" : "Mute Cam");
        lv_obj_set_style_bg_color(refs.privacyBtn, softPrivacy ? Ui::Success() : Ui::Danger(), LV_PART_MAIN);
    }
}

} // namespace

lv_obj_t * CreateCameraAvStreamManagementClusterWidget(lv_obj_t * parent, CameraAVStreamManagementCluster & cluster)
{
    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_hor(card, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(card, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, 4, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * streamsLabel = lv_label_create(card);
    lv_obj_set_style_text_font(streamsLabel, Ui::BodyFont(), LV_PART_MAIN);
    lv_obj_set_style_text_color(streamsLabel, Ui::Text(), LV_PART_MAIN);

    lv_obj_t * detailsLabel = lv_label_create(card);
    lv_obj_set_width(detailsLabel, LV_PCT(100));
    lv_obj_set_style_text_font(detailsLabel, Ui::CaptionFont(), LV_PART_MAIN);
    lv_obj_set_style_text_color(detailsLabel, Ui::TextMuted(), LV_PART_MAIN);
    lv_label_set_long_mode(detailsLabel, LV_LABEL_LONG_WRAP);

    lv_obj_t * privacyLabel    = nullptr;
    lv_obj_t * privacyBtn      = nullptr;
    lv_obj_t * privacyBtnLabel = nullptr;

    if (cluster.HasFeature(Feature::kPrivacy))
    {
        lv_obj_t * privacyRow = lv_obj_create(card);
        lv_obj_remove_style_all(privacyRow);
        lv_obj_set_width(privacyRow, LV_PCT(100));
        lv_obj_set_height(privacyRow, 34);
        lv_obj_set_flex_flow(privacyRow, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(privacyRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(privacyRow, LV_OBJ_FLAG_SCROLLABLE);

        privacyLabel = lv_label_create(privacyRow);

        privacyBtn = lv_button_create(privacyRow);
        lv_obj_set_width(privacyBtn, 104);
        lv_obj_set_height(privacyBtn, 30);
        lv_obj_set_flex_flow(privacyBtn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(privacyBtn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(privacyBtn, 2, LV_PART_MAIN);

        privacyBtnLabel = lv_label_create(privacyBtn);
        lv_obj_set_style_text_color(privacyBtnLabel, lv_color_white(), LV_PART_MAIN);

        lv_obj_add_event_cb(
            privacyBtn,
            [](lv_event_t * event) {
                auto * clusterPtr = static_cast<CameraAVStreamManagementCluster *>(lv_event_get_user_data(event));
                DeviceLayer::SystemLayer().ScheduleLambda([clusterPtr]() {
                    LogErrorOnFailure(
                        clusterPtr->SetSoftLivestreamPrivacyModeEnabled(!clusterPtr->GetSoftLivestreamPrivacyModeEnabled()));
                });
            },
            LV_EVENT_CLICKED, &cluster);
    }

    AvsmWidgetRefs refs{ &cluster, streamsLabel, detailsLabel, privacyLabel, privacyBtn, privacyBtnLabel };
    UpdateAvsmWidget(refs);

    DisplayNotificationHub::Instance().Subscribe(card, cluster.GetPaths()[0].mEndpointId, Clusters::CameraAvStreamManagement::Id,
                                                 [refs](const ConcreteAttributePath & path) {
                                                     switch (path.mAttributeId)
                                                     {
                                                     case Attributes::AllocatedVideoStreams::Id:
                                                     case Attributes::AllocatedAudioStreams::Id:
                                                     case Attributes::AllocatedSnapshotStreams::Id:
                                                     case Attributes::SoftLivestreamPrivacyModeEnabled::Id:
                                                     case Attributes::SoftRecordingPrivacyModeEnabled::Id:
                                                     case Attributes::HardPrivacyModeOn::Id:
                                                     case Attributes::CurrentFrameRate::Id:
                                                         UpdateAvsmWidget(refs);
                                                         break;
                                                     default:
                                                         break;
                                                     }
                                                 });

    return card;
}

} // namespace chip::app
