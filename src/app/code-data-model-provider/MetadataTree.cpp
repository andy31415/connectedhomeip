
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
#include "app/ConcreteClusterPath.h"
#include "app/ConcreteCommandPath.h"
#include "app/code-data-model-provider/Metadata.h"
#include "app/data-model-provider/MetadataTypes.h"
#include "lib/core/DataModelTypes.h"
#include <app/code-data-model-provider/MetadataTree.h>
#include <optional>

namespace chip {
namespace app {
namespace {

using SemanticTag = Clusters::Descriptor::Structs::SemanticTagStruct::Type;

/// Search for the index where `needle` inside `haystack`
///
/// using `compareFunc` to compare a needle with a haystack value.
template <typename N, typename H>
std::optional<size_t> FindIndexUsingHint(const N & needle, Span<H> haystack, size_t & hint,
                                         bool (*compareFunc)(const N &, const H &))
{
    if (hint < haystack.size())
    {
        if (compareFunc(needle, haystack[hint]))
        {
            return hint;
        }
    }

    for (size_t i = 0; i < haystack.size(); i++)
    {
        if (compareFunc(needle, haystack[i]))
        {
            hint = i;
            return i;
        }
    }

    return std::nullopt;
}

template <typename N, typename H>
std::optional<size_t> FindNextIndexUsingHint(const N & needle, Span<H> haystack, size_t & hint,
                                             bool (*compareFunc)(const N &, const H &))
{
    auto idx = FindIndexUsingHint<N, H>(needle, haystack, hint, compareFunc);
    if (!idx.has_value() || (*idx + 1 >= haystack.size()))
    {
        return std::nullopt;
    }
    hint = *idx + 1;
    return hint;
}

bool operator==(const Metadata::EndpointInstance::SemanticTag & tagA, const Metadata::EndpointInstance::SemanticTag & tagB)
{
    // Label is an optional and nullable value of CharSpan. Optional and Nullable have overload for ==,
    // But `==` is deleted for CharSpan. Here we only check whether the string is the same.
    if (tagA.label.HasValue() != tagB.label.HasValue())
    {
        return false;
    }
    if (tagA.label.HasValue())
    {
        if (tagA.label.Value().IsNull() != tagB.label.Value().IsNull())
        {
            return false;
        }
        if (!tagA.label.Value().IsNull())
        {
            if (!tagA.label.Value().Value().data_equal(tagB.label.Value().Value()))
            {
                return false;
            }
        }
    }
    return (tagA.tag == tagB.tag) && (tagA.mfgCode == tagB.mfgCode) && (tagA.namespaceID == tagB.namespaceID);
}

bool SameEndpointId(const EndpointId & id, const Metadata::EndpointInstance & instance)
{
    return id == instance.id;
}

template <typename T>
bool SameValue(const T & a, const T & b)
{
    return a == b;
}

bool SameClusterId(const ClusterId & id, const Metadata::ClusterInstance & instance)
{
    return id == instance.metadata->clusterId;
}

/// Convert a endpoint instance struct to a datamodel endpoint entry
DataModel::EndpointEntry EndpointEntryFrom(const Metadata::EndpointInstance & instance)
{
    return DataModel::EndpointEntry{
        .id   = instance.id,
        .info = DataModel::EndpointInfo(instance.parentEndpointId, instance.endpointComposition),
    };
}

DataModel::ClusterEntry ClusterEntryFrom(EndpointId endpointId, const Metadata::ClusterInstance & instance)
{
    DataModel::ClusterInfo clusterInfo(instance.dataVersion);

    clusterInfo.flags = instance.metadata->qualities;

    return DataModel::ClusterEntry{ .path = ConcreteClusterPath(endpointId, instance.metadata->clusterId), .info = clusterInfo };
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
    std::optional<size_t> idx = FindNextIndexUsingHint(before, mEndpoints, mEndpointIndexHint, SameEndpointId);
    return idx.has_value() ? EndpointEntryFrom(mEndpoints[*idx]) : DataModel::EndpointEntry::kInvalid;
}

std::optional<DataModel::EndpointInfo> CodeMetadataTree::GetEndpointInfo(EndpointId id)
{
    std::optional<size_t> index = FindIndexUsingHint(id, mEndpoints, mEndpointIndexHint, SameEndpointId);
    if (!index.has_value())
    {
        return std::nullopt;
    }
    return EndpointEntryFrom(mEndpoints[*index]).info;
}

std::optional<DataModel::DeviceTypeEntry> CodeMetadataTree::FirstDeviceType(EndpointId endpoint)
{
    std::optional<size_t> index = FindIndexUsingHint(endpoint, mEndpoints, mEndpointIndexHint, SameEndpointId);
    if (!index.has_value())
    {
        return std::nullopt;
    }

    auto & ep = mEndpoints[*index];
    if (ep.deviceTypes.empty())
    {
        return std::nullopt;
    }

    mDeviceTypeHint = 0;
    return ep.deviceTypes[0];
}

std::optional<DataModel::DeviceTypeEntry> CodeMetadataTree::NextDeviceType(EndpointId endpoint,
                                                                           const DataModel::DeviceTypeEntry & previous)
{
    std::optional<size_t> ep_index = FindIndexUsingHint(endpoint, mEndpoints, mEndpointIndexHint, SameEndpointId);
    if (!ep_index.has_value())
    {
        return std::nullopt;
    }

    auto & ep = mEndpoints[*ep_index];

    std::optional<size_t> idx = FindNextIndexUsingHint(previous, ep.deviceTypes, mDeviceTypeHint, SameValue);
    return idx.has_value() ? std::make_optional(ep.deviceTypes[*idx]) : std::nullopt;
}

std::optional<SemanticTag> CodeMetadataTree::GetFirstSemanticTag(EndpointId endpoint)
{
    std::optional<size_t> ep_index = FindIndexUsingHint(endpoint, mEndpoints, mEndpointIndexHint, SameEndpointId);
    if (!ep_index.has_value())
    {
        return std::nullopt;
    }

    auto & ep = mEndpoints[*ep_index];
    if (ep.semanticTags.empty())
    {
        return std::nullopt;
    }

    mSemanticTagHint = 0;
    return ep.semanticTags[0];
}

std::optional<SemanticTag> CodeMetadataTree::GetNextSemanticTag(EndpointId endpoint, const SemanticTag & previous)
{
    std::optional<size_t> ep_index = FindIndexUsingHint(endpoint, mEndpoints, mEndpointIndexHint, SameEndpointId);
    if (!ep_index.has_value())
    {
        return std::nullopt;
    }

    auto & ep = mEndpoints[*ep_index];

    std::optional<size_t> tagIndex = FindNextIndexUsingHint(previous, ep.semanticTags, mSemanticTagHint, SameValue);
    return tagIndex.has_value() ? std::make_optional(ep.semanticTags[*tagIndex]) : std::nullopt;
}

DataModel::ClusterEntry CodeMetadataTree::FirstServerCluster(EndpointId endpoint)
{
    std::optional<size_t> ep_index = FindIndexUsingHint(endpoint, mEndpoints, mEndpointIndexHint, SameEndpointId);
    if (!ep_index.has_value())
    {
        return DataModel::ClusterEntry::kInvalid;
    }
    auto & ep = mEndpoints[*ep_index];
    if (ep.serverClusters.empty())
    {
        return DataModel::ClusterEntry::kInvalid;
    }

    mServerClusterHint = 0;
    return ClusterEntryFrom(endpoint, ep.serverClusters[0]);
}

DataModel::ClusterEntry CodeMetadataTree::NextServerCluster(const ConcreteClusterPath & before)
{
    std::optional<size_t> ep_index = FindIndexUsingHint(before.mEndpointId, mEndpoints, mEndpointIndexHint, SameEndpointId);
    if (!ep_index.has_value())
    {
        return DataModel::ClusterEntry::kInvalid;
    }
    auto & ep = mEndpoints[*ep_index];
    std::optional<size_t> cluster_index =
        FindNextIndexUsingHint(before.mClusterId, ep.serverClusters, mServerClusterHint, SameClusterId);
    if (!cluster_index.has_value())
    {
        return DataModel::ClusterEntry::kInvalid;
    }

    return ClusterEntryFrom(before.mEndpointId, ep.serverClusters[*cluster_index]);
}

std::optional<DataModel::ClusterInfo> CodeMetadataTree::GetServerClusterInfo(const ConcreteClusterPath & path)
{
    std::optional<size_t> ep_index = FindIndexUsingHint(path.mEndpointId, mEndpoints, mEndpointIndexHint, SameEndpointId);
    if (!ep_index.has_value())
    {
        return std::nullopt;
    }
    auto & ep                           = mEndpoints[*ep_index];
    std::optional<size_t> cluster_index = FindIndexUsingHint(path.mClusterId, ep.serverClusters, mServerClusterHint, SameClusterId);
    if (!cluster_index.has_value())
    {
        return std::nullopt;
    }

    return ClusterEntryFrom(path.mEndpointId, ep.serverClusters[*cluster_index]).info;
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
