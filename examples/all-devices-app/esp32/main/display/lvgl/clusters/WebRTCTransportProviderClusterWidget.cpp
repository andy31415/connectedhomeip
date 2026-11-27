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

#include "WebRTCTransportProviderClusterWidget.h"
#include "DisplayNotificationHub.h"
#include "Ui.h"

#include <clusters/WebRTCTransportProvider/AttributeIds.h>
#include <clusters/WebRTCTransportProvider/Ids.h>
#include <lib/support/StringBuilder.h>

namespace chip::app {

namespace {

using namespace Clusters::WebRTCTransportProvider;

void UpdateWebRTCDisplay(lv_obj_t * statusLabel, lv_obj_t * sessionsLabel, WebRTCTransportProviderCluster & cluster)
{
    const auto sessions = cluster.GetCurrentSessions();
    if (sessions.empty())
    {
        lv_label_set_text_static(statusLabel, "WebRTC: IDLE");
        lv_obj_set_style_text_color(statusLabel, Ui::TextMuted(), LV_PART_MAIN);
        lv_label_set_text_static(sessionsLabel, "No active WebRTC sessions");
        return;
    }

    StringBuilder<48> header;
    header.AddFormat("WebRTC: ACTIVE (%u)", static_cast<unsigned int>(sessions.size()));
    lv_label_set_text(statusLabel, header.c_str());
    lv_obj_set_style_text_color(statusLabel, Ui::Success(), LV_PART_MAIN);

    StringBuilder<160> details;
    for (const auto & session : sessions)
    {
        const size_t vCount = session.videoStreams.HasValue() ? session.videoStreams.Value().size() : 0;
        const size_t aCount = session.audioStreams.HasValue() ? session.audioStreams.Value().size() : 0;
        details.AddFormat("%ss#%u  peer 0x%llX  V:%u A:%u", details.Empty() ? "" : "\n", static_cast<unsigned int>(session.id),
                          static_cast<unsigned long long>(session.peerNodeID), static_cast<unsigned int>(vCount),
                          static_cast<unsigned int>(aCount));
    }
    lv_label_set_text(sessionsLabel, details.c_str());
}

} // namespace

lv_obj_t * CreateWebRTCTransportProviderClusterWidget(lv_obj_t * parent, WebRTCTransportProviderCluster & cluster)
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

    lv_obj_t * statusLabel = lv_label_create(card);
    lv_obj_set_style_text_font(statusLabel, Ui::BodyFont(), LV_PART_MAIN);

    lv_obj_t * sessionsLabel = lv_label_create(card);
    lv_obj_set_width(sessionsLabel, LV_PCT(100));
    lv_obj_set_style_text_font(sessionsLabel, Ui::CaptionFont(), LV_PART_MAIN);
    lv_obj_set_style_text_color(sessionsLabel, Ui::TextMuted(), LV_PART_MAIN);
    lv_label_set_long_mode(sessionsLabel, LV_LABEL_LONG_WRAP);

    UpdateWebRTCDisplay(statusLabel, sessionsLabel, cluster);

    DisplayNotificationHub::Instance().Subscribe(card, cluster.GetPaths()[0].mEndpointId, Clusters::WebRTCTransportProvider::Id,
                                                 [statusLabel, sessionsLabel, &cluster](const ConcreteAttributePath & path) {
                                                     if (path.mAttributeId == Attributes::CurrentSessions::Id)
                                                     {
                                                         UpdateWebRTCDisplay(statusLabel, sessionsLabel, cluster);
                                                     }
                                                 });

    return card;
}

} // namespace chip::app
