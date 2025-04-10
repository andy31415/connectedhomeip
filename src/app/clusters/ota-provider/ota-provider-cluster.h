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
#pragma once

#include <app/server-cluster/DefaultServerCluster.h>
#include <clusters/OtaSoftwareUpdateProvider/ClusterId.h>

namespace chip {
namespace app {
namespace Clusters {

class OtaProviderCluster : public DefaultServerCluster
{
public:
    OtaProviderCluster() : DefaultServerCluster({ kInvalidEndpointId, OtaSoftwareUpdateProvider::Id }) {}

    DataModel::ActionReturnStatus ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                AttributeValueEncoder & encoder) override;

    CHIP_ERROR AcceptedCommands(const ConcreteClusterPath & path,
                                DataModel::ListBuilder<DataModel::AcceptedCommandEntry> & builder) override;
    CHIP_ERROR GeneratedCommands(const ConcreteClusterPath & path, DataModel::ListBuilder<CommandId> & builder) override;

    std::optional<DataModel::ActionReturnStatus> InvokeCommand(const DataModel::InvokeRequest & request,
                                                               chip::TLV::TLVReader & input_arguments,
                                                               CommandHandler * handler) override;

private:
    // TODO: what members are needed here?
    //   we likely need PATHS!
};

} // namespace Clusters
} // namespace app
} // namespace chip
