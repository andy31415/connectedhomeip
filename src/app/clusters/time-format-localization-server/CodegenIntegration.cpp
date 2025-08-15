/*
 *    Copyright (c) 2025 Project CHIP Authors
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
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app/clusters/time-format-localization-server/time-format-localization-cluster.h>
#include <app/static-cluster-config/TimeFormatLocalization.h>
#include <app/util/attribute-metadata.h>
#include <data-model-providers/codegen/CodegenDataModelProvider.h>
#include <platform/DeviceInfoProvider.h>
#include <platform/PlatformManager.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace Protocols::InteractionModel;

namespace {

LazyRegisteredServerCluster<TimeFormatLocalizationCluster> gServer;

}

void emberAfTimeFormatLocalizationClusterServerInitCallback(EndpointId endpoint)
{
    // This cluster should only exist in Root endpoint.
    VerifyOrReturn(endpoint == kRootEndpointId);

    // Get feature Map
    uint32_t rawFeatureMap;
    if (TimeFormatLocalization::Attributes::FeatureMap::Get(endpoint, &rawFeatureMap) != Status::Success)
    {
        ChipLogError(AppServer, "Failed to get feature map for endpoint %u", endpoint);
        rawFeatureMap = 0;
    }

    // Get the default values from ZAP, just in case of read failure we set the values to the initial of the enum (k12hr and
    // kBuddhist).
    TimeFormatLocalization::HourFormatEnum defaultHourFormat     = TimeFormatLocalization::HourFormatEnum::k12hr;
    TimeFormatLocalization::CalendarTypeEnum defaultCalendarType = TimeFormatLocalization::CalendarTypeEnum::kBuddhist;

    // First the HourFormat
    if (TimeFormatLocalization::Attributes::HourFormat::Get(endpoint, &defaultHourFormat) != Status::Success)
    {
        ChipLogError(AppServer, "Failed to get HourFormat for endpoint %u", endpoint);
        defaultHourFormat = TimeFormatLocalization::HourFormatEnum::k12hr;
    }

    // Get the ActiveCalendarType default value if the feature is enabled.
    if (BitFlags<TimeFormatLocalization::Feature>(rawFeatureMap).Has(TimeFormatLocalization::Feature::kCalendarFormat))
    {
        if (TimeFormatLocalization::Attributes::ActiveCalendarType::Get(endpoint, &defaultCalendarType) != Status::Success)
        {
            ChipLogError(AppServer, "Failed to get ActiveCalendarType for endpoint %u", endpoint);
            defaultCalendarType = TimeFormatLocalization::CalendarTypeEnum::kBuddhist;
        }
    }

    gServer.Create(endpoint, BitFlags<TimeFormatLocalization::Feature>(rawFeatureMap), defaultHourFormat, defaultCalendarType);

    CHIP_ERROR err = CodegenDataModelProvider::Instance().Registry().Register(gServer.Registration());

    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "TimeFormatLocalization cluster error registration");
    }
}

void MatterTimeFormatLocalizationClusterServerShutdownCallback(EndpointId endpoint)
{
    // This cluster should only exist in Root endpoint.
    VerifyOrReturn(endpoint == kRootEndpointId);

    CHIP_ERROR err = CodegenDataModelProvider::Instance().Registry().Unregister(&gServer.Cluster());

    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "TimeFormatLocalization unregister error");
    }

    gServer.Destroy();
}

void MatterTimeFormatLocalizationPluginServerInitCallback() {}
