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
#include <app/clusters/ota-provider/ota-provider-cluster.h>
#include <app/static-cluster-config/OtaSoftwareUpdateProvider.h>
#include <app/util/attribute-storage.h>
#include <data-model-providers/codegen/CodegenDataModelProvider.h>
#include <data-model-providers/codegen/Instance.h>

#include <array>

using namespace chip::app;
using namespace chip::app::Clusters;

static constexpr size_t kOtaProviderMaxClusterCount =
    OtaSoftwareUpdateProvider::StaticApplicationConfig::kFixedClusterConfig.size() + CHIP_DEVICE_CONFIG_DYNAMIC_ENDPOINT_COUNT;

std::array<OtaProviderServer, kOtaProviderMaxClusterCount> mClusters;
std::array<ServerClusterRegistration, kOtaProviderMaxClusterCount> mRegistrations;

void emberAfOtaSoftwareUpdateProviderClusterServerInitCallback(chip::EndpointId endpoint)
{
    uint16_t arrayIndex = emberAfGetClusterServerEndpointIndex(
        endpoint, OtaSoftwareUpdateProvider::Id, OtaSoftwareUpdateProvider::StaticApplicationConfig::kFixedClusterConfig.size());

    if (arrayIndex >= kOtaProviderMaxClusterCount)
    {
        ChipLogError(AppServer, "Endpoint startup: OTA endpoint %u unsupported", endpoint);
        return;
    }
    mClusters[arrayIndex].SetEndpointId(endpoint);
    // TODO: codegen cast should be fixed

    auto * instance = static_cast<CodegenDataModelProvider *>(chip::app::CodegenDataModelProviderInstance(nullptr));

    mRegistrations[arrayIndex].serverClusterInterface = &mClusters[arrayIndex];

    CHIP_ERROR err = instance->Registry().Register(mRegistrations[arrayIndex]);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "Failed to register OTA on endpoint %u: %" CHIP_ERROR_FORMAT, endpoint, err.Format());
    }
}

void emberAfOtaSoftwareUpdateProviderClusterShutdownCallback(chip::EndpointId endpoint)
{
    uint16_t arrayIndex = emberAfGetClusterServerEndpointIndex(
        endpoint, OtaSoftwareUpdateProvider::Id, MATTER_DM_OTA_SOFTWARE_UPDATE_PROVIDER_CLUSTER_SERVER_ENDPOINT_COUNT);

    if (arrayIndex >= kOtaProviderMaxClusterCount)
    {
        ChipLogError(AppServer, "Endpoint startup: OTA endpoint %u unsupported", endpoint);
        return;
    }

    auto * instance = static_cast<CodegenDataModelProvider *>(chip::app::CodegenDataModelProviderInstance(nullptr));
    CHIP_ERROR err  = instance->Registry().Unregister(&mClusters[arrayIndex]);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(AppServer, "Failed to unregister OTA on endpoint %u: %" CHIP_ERROR_FORMAT, endpoint, err.Format());
    }
}

void MatterOtaSoftwareUpdateProviderPluginServerInitCallback() {}
void MatterOtaSoftwareUpdateProviderPluginServerShutdownCallback() {}

namespace chip {
namespace app {
namespace Clusters {
namespace OTAProvider {

void SetDelegate(chip::EndpointId endpointId, OTAProviderDelegate * delegate) {
    uint16_t arrayIndex = emberAfGetClusterServerEndpointIndex(
        endpointId, OtaSoftwareUpdateProvider::Id, MATTER_DM_OTA_SOFTWARE_UPDATE_PROVIDER_CLUSTER_SERVER_ENDPOINT_COUNT);

    if (arrayIndex >= kOtaProviderMaxClusterCount)
    {
        ChipLogError(AppServer, "Set delegate startup: OTA endpoint %u unsupported", endpointId);
        return;
    }
    mClusters[arrayIndex].SetDelegate(delegate);

}

} // namespace OTAProvider
} // namespace Clusters
} // namespace app
} // namespace chip
