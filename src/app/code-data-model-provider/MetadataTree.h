/*
 *    Copyright (c) 2024 Project CHIP Authors
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

#include <app/code-data-model-provider/Metadata.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <lib/support/Span.h>

namespace chip {
namespace app {

class CodeMetadataTree : public DataModel::ProviderMetadataTree
{
public:
    CodeMetadataTree(Span<Metadata::EndpointInstance> endpoints) : mEndpoints(endpoints) {}

    /////// Implementation of ProviderMetadataTree  /////////
    using SemanticTag = Clusters::Descriptor::Structs::SemanticTagStruct::Type;

    DataModel::EndpointEntry FirstEndpoint() override;
    DataModel::EndpointEntry NextEndpoint(EndpointId before) override;
    std::optional<DataModel::EndpointInfo> GetEndpointInfo(EndpointId id) override;
    std::optional<DataModel::DeviceTypeEntry> FirstDeviceType(EndpointId endpoint) override;
    std::optional<DataModel::DeviceTypeEntry> NextDeviceType(EndpointId endpoint,
                                                             const DataModel::DeviceTypeEntry & previous) override;
    std::optional<SemanticTag> GetFirstSemanticTag(EndpointId endpoint) override;
    std::optional<SemanticTag> GetNextSemanticTag(EndpointId endpoint, const SemanticTag & previous) override;
    DataModel::ClusterEntry FirstServerCluster(EndpointId endpoint) override;
    DataModel::ClusterEntry NextServerCluster(const ConcreteClusterPath & before) override;
    std::optional<DataModel::ClusterInfo> GetServerClusterInfo(const ConcreteClusterPath & path) override;
    ConcreteClusterPath FirstClientCluster(EndpointId endpoint) override;
    ConcreteClusterPath NextClientCluster(const ConcreteClusterPath & before) override;
    DataModel::AttributeEntry FirstAttribute(const ConcreteClusterPath & cluster) override;
    DataModel::AttributeEntry NextAttribute(const ConcreteAttributePath & before) override;
    std::optional<DataModel::AttributeInfo> GetAttributeInfo(const ConcreteAttributePath & path) override;
    DataModel::CommandEntry FirstAcceptedCommand(const ConcreteClusterPath & cluster) override;
    DataModel::CommandEntry NextAcceptedCommand(const ConcreteCommandPath & before) override;
    std::optional<DataModel::CommandInfo> GetAcceptedCommandInfo(const ConcreteCommandPath & path) override;
    ConcreteCommandPath FirstGeneratedCommand(const ConcreteClusterPath & cluster) override;
    ConcreteCommandPath NextGeneratedCommand(const ConcreteCommandPath & before) override;

private:
    Span<Metadata::EndpointInstance> mEndpoints;
    size_t mEndpointIndexHint   = 0;
    size_t mDeviceTypeHint      = 0;
    size_t mSemanticTagHint     = 0;
    size_t mServerClusterHint   = 0;
    size_t mClientClusterHint   = 0;
    size_t mAttributeHint       = 0;
    size_t mAcceptedCommandHint = 0;
};

} // namespace app
} // namespace chip
