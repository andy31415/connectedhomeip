
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
#include <app/ConcreteAttributePath.h>
#include <app/ConcreteClusterPath.h>
#include <app/ConcreteCommandPath.h>
#include <app/code-data-model-provider/Metadata.h>
#include <app/code-data-model-provider/MetadataTree.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <lib/core/DataModelTypes.h>

#include <optional>
#include <type_traits>

namespace chip {
namespace app {
namespace {

using SemanticTag = Clusters::Descriptor::Structs::SemanticTagStruct::Type;

/// Search for the index where `needle` inside `haystack`
///
/// using `compareFunc` to compare a needle with a haystack value.
template <typename N, typename H>
std::optional<size_t> FindIndexUsingHint(const N & needle, Span<H> haystack, size_t & hint,
                                         bool (*compareFunc)(const N &, const typename std::remove_const<H>::type &))
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
                                             bool (*compareFunc)(const N &, const typename std::remove_const<H>::type &))
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

struct EndpointsWrapper
{
    Span<Metadata::EndpointInstance> endpoints;
    explicit EndpointsWrapper(Span<Metadata::EndpointInstance> e) : endpoints(e) {}
};

/// search endpoints by an endpoint ID
struct ByEndpoint
{
    using Key  = EndpointId;
    using Type = Metadata::EndpointInstance;
    static Span<Type> GetSpan(EndpointsWrapper & wrapper) { return wrapper.endpoints; }
    static bool Compare(const EndpointId & id, const Metadata::EndpointInstance & instance) { return id == instance.id; }
};

struct ByDeviceType
{
    using Key  = const DataModel::DeviceTypeEntry;
    using Type = const DataModel::DeviceTypeEntry;
    static Span<Type> GetSpan(Metadata::EndpointInstance & v) { return v.deviceTypes; }
    static bool Compare(const DataModel::DeviceTypeEntry & a, const DataModel::DeviceTypeEntry & b) { return a == b; }
};

struct BySemanticTag
{
    using Key  = const SemanticTag;
    using Type = const SemanticTag;
    static Span<Type> GetSpan(Metadata::EndpointInstance & v) { return v.semanticTags; }
    static bool Compare(const SemanticTag & a, const SemanticTag & b) { return a == b; }
};

struct ByServerCluster
{
    using Key  = ClusterId;
    using Type = Metadata::ClusterInstance;
    static Span<Type> GetSpan(Metadata::EndpointInstance & v) { return v.serverClusters; }
    static bool Compare(const ClusterId & id, const Metadata::ClusterInstance & instance)
    {
        return id == instance.metadata->clusterId;
    }
};

struct ByClientCluster
{
    using Key  = ClusterId;
    using Type = const ClusterId;
    static Span<Type> GetSpan(Metadata::EndpointInstance & v) { return v.clientClusters; }
    static bool Compare(const ClusterId & a, const ClusterId & b) { return a == b; }
};

struct ByAttribute
{
    using Key  = AttributeId;
    using Type = const Metadata::AttributeMeta;
    static Span<Type> GetSpan(Metadata::ClusterInstance & v) { return v.metadata->attributes; }
    static bool Compare(const AttributeId & id, const Metadata::AttributeMeta & meta) { return id == meta.id; }
};

/// represents a wrapper around a type `T` that contains internal
/// `Span<...>` values of other sub-types. It allows searching within the container sub-spans
/// to create new containers.
///
/// Use case is that we very often search within a tree, like "find-endpoint" + "find-cluster" + "find-attribute"
/// and we generally only care if "does the last element exist or not"
template <typename T>
class SearchableContainer
{
public:
    explicit SearchableContainer(T * value) : mValue(value) {}

    T * Value() const { return mValue; }

    // Get the first element of `TYPE`
    template <typename TYPE>
    SearchableContainer<typename TYPE::Type> First(size_t & indexHint)
    {
        // if no value, searching more also yields no value
        VerifyOrReturnValue(mValue != nullptr, SearchableContainer<typename TYPE::Type>(nullptr));

        Span<typename TYPE::Type> value_span = TYPE::GetSpan(*mValue);
        VerifyOrReturnValue(!value_span.empty(), SearchableContainer<typename TYPE::Type>(nullptr));

        // found it, save the hint
        indexHint = 0;
        return SearchableContainer<typename TYPE::Type>(&value_span[0]);
    }

    // Find the value for type EXACTLY type
    template <typename TYPE>
    SearchableContainer<typename TYPE::Type> Find(typename TYPE::Key key, size_t & indexHint)
    {
        VerifyOrReturnValue(mValue != nullptr, SearchableContainer<typename TYPE::Type>(nullptr));

        Span<typename TYPE::Type> value_span = TYPE::GetSpan(*mValue);
        std::optional<size_t> idx            = FindIndexUsingHint(key, value_span, indexHint, TYPE::Compare);

        VerifyOrReturnValue(idx.has_value(), SearchableContainer<typename TYPE::Type>(nullptr));
        return SearchableContainer<typename TYPE::Type>(&value_span[*idx]);
    }

    template <typename TYPE>
    SearchableContainer<typename TYPE::Type> Next(typename TYPE::Key key, size_t & indexHint)
    {
        VerifyOrReturnValue(mValue != nullptr, SearchableContainer<typename TYPE::Type>(nullptr));

        Span<typename TYPE::Type> value_span = TYPE::GetSpan(*mValue);
        std::optional<size_t> idx            = FindNextIndexUsingHint(key, value_span, indexHint, TYPE::Compare);

        VerifyOrReturnValue(idx.has_value(), SearchableContainer<typename TYPE::Type>(nullptr));
        return SearchableContainer<typename TYPE::Type>(&value_span[*idx]);
    }

private:
    T * mValue = nullptr; // underlying value, NULL if such a value does not exist
};

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

DataModel::AttributeEntry AttributeEntryFrom(const ConcreteClusterPath & clusterPath, const Metadata::AttributeMeta & attribute)
{
    return DataModel::AttributeEntry{
        .path = ConcreteAttributePath(clusterPath.mEndpointId, clusterPath.mClusterId, attribute.id),
        .info =
            DataModel::AttributeInfo{
                .flags          = attribute.qualities,
                .readPrivilege  = Metadata::ReadPrivilege(attribute.privileges),
                .writePrivilege = Metadata::WritePrivilege(attribute.privileges),
            },
    };
}

} // namespace

DataModel::EndpointEntry CodeMetadataTree::FirstEndpoint()
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    Metadata::EndpointInstance * ep = search.First<ByEndpoint>(mEndpointIndexHint).Value();

    return (ep == nullptr) ? DataModel::EndpointEntry::kInvalid : EndpointEntryFrom(*ep);
}

DataModel::EndpointEntry CodeMetadataTree::NextEndpoint(EndpointId before)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    Metadata::EndpointInstance * ep = search.Next<ByEndpoint>(before, mEndpointIndexHint).Value();

    return (ep == nullptr) ? DataModel::EndpointEntry::kInvalid : EndpointEntryFrom(*ep);
}

std::optional<DataModel::EndpointInfo> CodeMetadataTree::GetEndpointInfo(EndpointId id)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    Metadata::EndpointInstance * ep = search.Find<ByEndpoint>(id, mEndpointIndexHint).Value();

    return (ep == nullptr) ? std::nullopt : std::make_optional(EndpointEntryFrom(*ep).info);
}

std::optional<DataModel::DeviceTypeEntry> CodeMetadataTree::FirstDeviceType(EndpointId endpoint)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    const DataModel::DeviceTypeEntry * value = search                                              //
                                                   .Find<ByEndpoint>(endpoint, mEndpointIndexHint) //
                                                   .First<ByDeviceType>(mDeviceTypeHint)
                                                   .Value();

    return (value == nullptr) ? std::nullopt : std::make_optional(*value);
}

std::optional<DataModel::DeviceTypeEntry> CodeMetadataTree::NextDeviceType(EndpointId endpoint,
                                                                           const DataModel::DeviceTypeEntry & previous)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    const DataModel::DeviceTypeEntry * value = search                                              //
                                                   .Find<ByEndpoint>(endpoint, mEndpointIndexHint) //
                                                   .Next<ByDeviceType>(previous, mDeviceTypeHint)
                                                   .Value();

    return (value == nullptr) ? std::nullopt : std::make_optional(*value);
}

std::optional<SemanticTag> CodeMetadataTree::GetFirstSemanticTag(EndpointId endpoint)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    const SemanticTag * value = search                                              //
                                    .Find<ByEndpoint>(endpoint, mEndpointIndexHint) //
                                    .First<BySemanticTag>(mSemanticTagHint)
                                    .Value();

    return (value == nullptr) ? std::nullopt : std::make_optional(*value);
}

std::optional<SemanticTag> CodeMetadataTree::GetNextSemanticTag(EndpointId endpoint, const SemanticTag & previous)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    const SemanticTag * value = search                                              //
                                    .Find<ByEndpoint>(endpoint, mEndpointIndexHint) //
                                    .Next<BySemanticTag>(previous, mSemanticTagHint)
                                    .Value();

    return (value == nullptr) ? std::nullopt : std::make_optional(*value);
}

DataModel::ClusterEntry CodeMetadataTree::FirstServerCluster(EndpointId endpoint)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    const Metadata::ClusterInstance * value = search                                              //
                                                  .Find<ByEndpoint>(endpoint, mEndpointIndexHint) //
                                                  .First<ByServerCluster>(mServerClusterHint)
                                                  .Value();

    return (value == nullptr) ? DataModel::ClusterEntry::kInvalid : ClusterEntryFrom(endpoint, *value);
}

DataModel::ClusterEntry CodeMetadataTree::NextServerCluster(const ConcreteClusterPath & before)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    const Metadata::ClusterInstance * value = search                                                        //
                                                  .Find<ByEndpoint>(before.mEndpointId, mEndpointIndexHint) //
                                                  .Next<ByServerCluster>(before.mClusterId, mServerClusterHint)
                                                  .Value();

    return (value == nullptr) ? DataModel::ClusterEntry::kInvalid : ClusterEntryFrom(before.mEndpointId, *value);
}

std::optional<DataModel::ClusterInfo> CodeMetadataTree::GetServerClusterInfo(const ConcreteClusterPath & path)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    const Metadata::ClusterInstance * value = search                                                      //
                                                  .Find<ByEndpoint>(path.mEndpointId, mEndpointIndexHint) //
                                                  .Find<ByServerCluster>(path.mClusterId, mServerClusterHint)
                                                  .Value();

    return (value == nullptr) ? std::nullopt : std::make_optional(ClusterEntryFrom(path.mEndpointId, *value).info);
}

ConcreteClusterPath CodeMetadataTree::FirstClientCluster(EndpointId endpoint)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    const ClusterId * value = search                                              //
                                  .Find<ByEndpoint>(endpoint, mEndpointIndexHint) //
                                  .First<ByClientCluster>(mClientClusterHint)
                                  .Value();

    return (value == nullptr) ? ConcreteClusterPath() : ConcreteClusterPath(endpoint, *value);
}

ConcreteClusterPath CodeMetadataTree::NextClientCluster(const ConcreteClusterPath & before)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    const ClusterId * value = search                                                        //
                                  .Find<ByEndpoint>(before.mEndpointId, mEndpointIndexHint) //
                                  .Next<ByClientCluster>(before.mClusterId, mClientClusterHint)
                                  .Value();

    return (value == nullptr) ? ConcreteClusterPath() : ConcreteClusterPath(before.mEndpointId, *value);
}

DataModel::AttributeEntry CodeMetadataTree::FirstAttribute(const ConcreteClusterPath & clusterPath)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    const Metadata::AttributeMeta * value = search                                                                 //
                                                .Find<ByEndpoint>(clusterPath.mEndpointId, mEndpointIndexHint)     //
                                                .Find<ByServerCluster>(clusterPath.mClusterId, mServerClusterHint) //
                                                .First<ByAttribute>(mAttributeHint)                                //
                                                .Value();

    return (value == nullptr) ? DataModel::AttributeEntry::kInvalid : AttributeEntryFrom(clusterPath, *value);
}

DataModel::AttributeEntry CodeMetadataTree::NextAttribute(const ConcreteAttributePath & before)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    const Metadata::AttributeMeta * value = search                                                            //
                                                .Find<ByEndpoint>(before.mEndpointId, mEndpointIndexHint)     //
                                                .Find<ByServerCluster>(before.mClusterId, mServerClusterHint) //
                                                .Next<ByAttribute>(before.mAttributeId, mAttributeHint)       //
                                                .Value();

    return (value == nullptr) ? DataModel::AttributeEntry::kInvalid : AttributeEntryFrom(before, *value);
}

std::optional<DataModel::AttributeInfo> CodeMetadataTree::GetAttributeInfo(const ConcreteAttributePath & path)
{
    EndpointsWrapper wrapper(mEndpoints);
    SearchableContainer<EndpointsWrapper> search(&wrapper);

    const Metadata::AttributeMeta * value = search                                                          //
                                                .Find<ByEndpoint>(path.mEndpointId, mEndpointIndexHint)     //
                                                .Find<ByServerCluster>(path.mClusterId, mServerClusterHint) //
                                                .Find<ByAttribute>(path.mAttributeId, mAttributeHint)       //
                                                .Value();

    return (value == nullptr) ? std::nullopt : std::make_optional(AttributeEntryFrom(path, *value).info);
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
