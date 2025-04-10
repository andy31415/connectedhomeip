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

#include <clusters/OtaSoftwareUpdateProvider/Ids.h>
#include <clusters/OtaSoftwareUpdateProvider/Metadata.h>

namespace chip {
namespace app {
namespace Clusters {

using Protocols::InteractionModel::Status;
using namespace OtaSoftwareUpdateProvider::Commands;

DataModel::ActionReturnStatus OtaProviderCluster::ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                                AttributeValueEncoder & encoder)
{
    switch (request.path.mAttributeId)
    {
    case Globals::Attributes::FeatureMap::Id:
        // No features defined for the cluster ...
        return encoder.Encode(0);
    case Globals::Attributes::ClusterRevision::Id:
        return encoder.Encode(OtaSoftwareUpdateProvider::Revision);
    }

    return Status::UnsupportedAttribute;
}

CHIP_ERROR OtaProviderCluster::AcceptedCommands(const ConcreteClusterPath & path,
                                                DataModel::ListBuilder<DataModel::AcceptedCommandEntry> & builder)
{
    static constexpr DataModel::AcceptedCommandEntry kEntries[] = {
        QueryImage::kMetatadaEntry,
        ApplyUpdateRequest::kMetatadaEntry,
        NotifyUpdateApplied::kMetatadaEntry,
    };
    return builder.ReferenceExisting({ kEntries, MATTER_ARRAY_SIZE(kEntries) });
}
CHIP_ERROR OtaProviderCluster::GeneratedCommands(const ConcreteClusterPath & path, DataModel::ListBuilder<CommandId> & builder)
{
    static constexpr CommandId kEntries[] = {
        OtaSoftwareUpdateProvider::Commands::QueryImageResponse::Id,
        OtaSoftwareUpdateProvider::Commands::ApplyUpdateResponse::Id,
    };
    return builder.ReferenceExisting({ kEntries, MATTER_ARRAY_SIZE(kEntries) });
}

std::optional<DataModel::ActionReturnStatus> OtaProviderCluster::InvokeCommand(const DataModel::InvokeRequest & request,
                                                                               chip::TLV::TLVReader & input_arguments,
                                                                               CommandHandler * handler)
{
    // TODO: implement
    return Status::UnsupportedCommand;
}

} // namespace Clusters
} // namespace app
} // namespace chip
