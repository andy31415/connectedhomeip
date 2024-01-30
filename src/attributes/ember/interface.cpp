/*
 *
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
#include <attributes/ember/interface.h>
#include <lib/support/CodeUtils.h>

// TODO: we SHOULD use includes from ember and dependencies,
//       however EMBER depends on zap-generated bits and as such three are
//       no valid include paths.

#include <app/util/af-types.h>
#include <app/util/attribute-metadata.h>

// Extern declarations because includes cannot be fixed. We could not include items
// such as:
//     #include <app/util/af.h>
//     #include <app/util/attribute-storage.h>
//     #include <app/util/attribute-table.h>
// TODO: proper includes should be fixed
uint16_t emberAfIndexFromEndpointIncludingDisabledEndpoints(chip::EndpointId endpoint);
uint8_t emberAfClusterCount(chip::EndpointId endpoint, bool server);
const EmberAfCluster * emberAfGetNthCluster(chip::EndpointId endpoint, uint8_t n, bool server);
bool emberAfEndpointIndexIsEnabled(uint16_t index);
chip::EndpointId emberAfEndpointFromIndex(uint16_t index);

// Even constants declared in headers we cannot include
static constexpr uint16_t kEmberInvalidEndpointIndex = 0xFFFF;

namespace chip {
namespace Attributes {

Endpoint::Index EmberDatabase::IndexOf(Endpoint::Id id)
{
    uint16_t endpoint_index = emberAfIndexFromEndpointIncludingDisabledEndpoints(id.Raw());
    if (endpoint_index == kEmberInvalidEndpointIndex)
    {
        return Endpoint::Index::Invalid();
    }

    return Endpoint::Index(endpoint_index);
}

Cluster::IndexPath EmberDatabase::IndexOf(Cluster::Path path)
{
    VerifyOrReturnValue(path.IsValid(), Cluster::IndexPath::Invalid());

    Endpoint::Index endpoint_index = IndexOf(path.GetEndpoint());
    VerifyOrReturnValue(endpoint_index.IsValid(), Cluster::IndexPath::Invalid());

    const uint8_t cluster_count = emberAfClusterCount(path.GetEndpoint().Raw(), /* server = */ true);
    for (uint8_t i = 0; i < cluster_count; i++)
    {
        const EmberAfCluster * cluster = emberAfGetNthCluster(path.GetEndpoint().Raw(), i, /* server = */ true);

        VerifyOrDie(cluster != nullptr);
        if (cluster->clusterId != path.GetCluster().Raw())
        {
            continue;
        }

        return Cluster::IndexPath(endpoint_index, Cluster::Index(i));
    }

    return Cluster::IndexPath::Invalid();
}

Attribute::IndexPath EmberDatabase::IndexOf(Attribute::Path path)
{
    VerifyOrReturnValue(path.IsValid(), Attribute::IndexPath::Invalid());

    Cluster::IndexPath cluster_index = IndexOf(path.GetClusterPath());
    VerifyOrReturnValue(cluster_index.IsValid(), Attribute::IndexPath::Invalid());

    const EmberAfCluster * cluster =
        emberAfGetNthCluster(path.GetEndpoint().Raw(), cluster_index.GetCluster().Raw(), /* server = */ true);

    VerifyOrDie(cluster != nullptr);
    VerifyOrDie(cluster->clusterId == path.GetCluster().Raw());

    const uint16_t attribute_count = cluster->attributeCount;
    for (uint16_t i = 0; i < attribute_count; i++)
    {
        const EmberAfAttributeMetadata & attribute = cluster->attributes[i];
        if (attribute.attributeId != path.GetAttribute().Raw())
        {
            continue;
        }

        return Attribute::IndexPath(cluster_index, Attribute::Index(i));
    }

    return Attribute::IndexPath::Invalid();
}

Endpoint::Id EmberDatabase::IdForPath(Endpoint::Index idx)
{
    VerifyOrReturnValue(idx.IsValid(), Endpoint::Id::Invalid());

    return Endpoint::Id(emberAfEndpointFromIndex(idx.Raw()));
}

Cluster::Path EmberDatabase::IdForPath(Cluster::IndexPath)
{
    // TODO
    return Cluster::Path::Invalid();
}

Attribute::Path EmberDatabase::IdForPath(Attribute::IndexPath)
{
    // TODO
    return Attribute::Path::Invalid();
}

Endpoint::Index EmberDatabase::EndpointEnd()
{
    // TODO
    return Endpoint::Index(0);
}

Cluster::Index EmberDatabase::ClusterEnd(Endpoint::Index)
{
    // TODO
    return Cluster::Index(0);
}

Attribute::Index EmberDatabase::AttributeEnd(Cluster::IndexPath)
{
    // TODO
    return Attribute::Index(0);
}

bool EmberDatabase::IsEnabled(Endpoint::Id id)
{
    VerifyOrReturnValue(id.IsValid(), false);
    return IsEnabled(IndexOf(id));
}

bool EmberDatabase::IsEnabled(Cluster::Path)
{
    // TODO
    return false;
}

bool EmberDatabase::IsEnabled(Endpoint::Index idx)
{
    VerifyOrReturnValue(idx.IsValid(), false);
    return emberAfEndpointIndexIsEnabled(idx.Raw());
}

bool EmberDatabase::IsEnabled(Cluster::IndexPath)
{
    // TODO
    return false;
}

CHIP_ERROR EmberDatabase::WriteAttribute(Attribute::IndexPath attribute, ByteSpan data, DataType type)
{
    // TODO
    return CHIP_ERROR_NOT_IMPLEMENTED;
}
CHIP_ERROR EmberDatabase::ReadAttribute(Attribute::IndexPath attribute, MutableByteSpan & data, DataType & type)
{
    // TODO
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

} // namespace Attributes
} // namespace chip
