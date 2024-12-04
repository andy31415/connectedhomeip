
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
#include "app/ConcreteCommandPath.h"
#include "app/code-data-model-provider/Metadata.h"
#include "app/data-model-provider/MetadataTypes.h"
#include <app/code-data-model-provider/MetadataTree.h>
#include <optional>

namespace chip {
namespace app {
namespace {
using SemanticTag = Clusters::Descriptor::Structs::SemanticTagStruct::Type;

/// Try to find the endpoint with the given ID in a span
/// Uses a `hint` to speed up searches
std::optional<size_t> FindEndpointIndex(EndpointId id, Span<Metadata::EndpointInstance> endpoints, size_t & hint)
{
    if (hint < endpoints.size())
    {
        // see if hint is a good index, maybe we are lucky
        if (endpoints[hint].id == id)
        {
            return hint;
        }
    }

    // hint was not useful, just search the whole thing
    for (size_t i = 0; i < endpoints.size(); i++)
    {
        if (endpoints[i].id == id)
        {
            hint = i;
            return i;
        }
    }

    return std::nullopt;
}

/// Convert a endpoint instance struct to a datamodel endpoint entry
DataModel::EndpointEntry EndpointEntryFrom(const Metadata::EndpointInstance & instance)
{
    return DataModel::EndpointEntry{
        .id   = instance.id,
        .info = DataModel::EndpointInfo(instance.parentEndpointId, instance.endpointComposition),
    };
}

} // namespace

DataModel::EndpointEntry CodeMetadataTree::FirstEndpoint()
{
    if (mEndpoints.empty())
    {
        return DataModel::EndpointEntry::kInvalid;
    }

    return EndpointEntryFrom(mEndpoints[0]);
}

DataModel::EndpointEntry CodeMetadataTree::NextEndpoint(EndpointId before)
{
    std::optional<size_t> beforeIndex = FindEndpointIndex(before, mEndpoints, mEndpointIndexHint);
    if (!beforeIndex.has_value())
    {
        return DataModel::EndpointEntry::kInvalid;
    }

    if (*beforeIndex + 1 >= mEndpoints.size())
    {
        // reached the end
        return DataModel::EndpointEntry::kInvalid;
    }

    return EndpointEntryFrom(mEndpoints[*beforeIndex + 1]);
}

std::optional<DataModel::EndpointInfo> CodeMetadataTree::GetEndpointInfo(EndpointId id)
{
    std::optional<size_t> index = FindEndpointIndex(id, mEndpoints, mEndpointIndexHint);
    if (!index.has_value())
    {
        return std::nullopt;
    }
    return EndpointEntryFrom(mEndpoints[*index]).info;
}

std::optional<DataModel::DeviceTypeEntry> CodeMetadataTree::FirstDeviceType(EndpointId endpoint)
{
    std::optional<size_t> index = FindEndpointIndex(endpoint, mEndpoints, mEndpointIndexHint);
    if (!index.has_value())
    {
        return std::nullopt;
    }

    auto & ep = mEndpoints[*index];
    if (ep.deviceTypes.empty())
    {
        return std::nullopt;
    }

    return ep.deviceTypes[0];
}

std::optional<DataModel::DeviceTypeEntry> CodeMetadataTree::NextDeviceType(EndpointId endpoint,
                                                                           const DataModel::DeviceTypeEntry & previous)
{
    // FIXME: implement
    return std::nullopt;
}

std::optional<SemanticTag> CodeMetadataTree::GetFirstSemanticTag(EndpointId endpoint)
{
    std::optional<size_t> index = FindEndpointIndex(endpoint, mEndpoints, mEndpointIndexHint);
    if (!index.has_value())
    {
        return std::nullopt;
    }

    auto & ep = mEndpoints[*index];
    if (ep.semanticTags.empty())
    {
        return std::nullopt;
    }

    return ep.semanticTags[0];
}

std::optional<SemanticTag> CodeMetadataTree::GetNextSemanticTag(EndpointId endpoint, const SemanticTag & previous)
{
    // FIXME: implement
    return std::nullopt;
}

DataModel::ClusterEntry CodeMetadataTree::FirstServerCluster(EndpointId endpoint)
{
    // FIXME: implement
    return DataModel::ClusterEntry::kInvalid;
}

DataModel::ClusterEntry CodeMetadataTree::NextServerCluster(const ConcreteClusterPath & before)
{
    // FIXME: implement
    return DataModel::ClusterEntry::kInvalid;
}

std::optional<DataModel::ClusterInfo> CodeMetadataTree::GetServerClusterInfo(const ConcreteClusterPath & path)
{
    // FIXME: implement
    return std::nullopt;
}

ConcreteClusterPath CodeMetadataTree::FirstClientCluster(EndpointId endpoint)
{
    // FIXME: implement
    return {};
}

ConcreteClusterPath CodeMetadataTree::NextClientCluster(const ConcreteClusterPath & before)
{
    // FIXME: implement
    return {};
}

DataModel::AttributeEntry CodeMetadataTree::FirstAttribute(const ConcreteClusterPath & cluster)
{
    // FIXME: implement
    return DataModel::AttributeEntry::kInvalid;
}

DataModel::AttributeEntry CodeMetadataTree::NextAttribute(const ConcreteAttributePath & before)
{
    // FIXME: implement
    return DataModel::AttributeEntry::kInvalid;
}

std::optional<DataModel::AttributeInfo> CodeMetadataTree::GetAttributeInfo(const ConcreteAttributePath & path)
{
    // FIXME: implement
    return std::nullopt;
}

DataModel::CommandEntry CodeMetadataTree::FirstAcceptedCommand(const ConcreteClusterPath & cluster)
{
    // FIXME: implement
    return DataModel::CommandEntry::kInvalid;
}

DataModel::CommandEntry CodeMetadataTree::NextAcceptedCommand(const ConcreteCommandPath & before)
{
    // FIXME: implement
    return DataModel::CommandEntry::kInvalid;
}

std::optional<DataModel::CommandInfo> CodeMetadataTree::GetAcceptedCommandInfo(const ConcreteCommandPath & path)
{
    // FIXME: implement
    return std::nullopt;
}

ConcreteCommandPath CodeMetadataTree::FirstGeneratedCommand(const ConcreteClusterPath & cluster)
{
    // FIXME: implement
    return {};
}

ConcreteCommandPath CodeMetadataTree::NextGeneratedCommand(const ConcreteCommandPath & before)
{
    // FIXME: implement
    return {};
}

} // namespace app
} // namespace chip
