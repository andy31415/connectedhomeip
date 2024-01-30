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

#include <app/GlobalAttributes.h>

// TODO: we SHOULD use includes from ember and dependencies,
#include <app/util/af-types.h>
#include <app/util/attribute-metadata.h>
#include <app/util/error-mapping.h>

// Extern declarations because includes cannot be fixed. We could not include items
// such as:
//     #include <app/util/af.h>
//     #include <app/util/attribute-storage.h>
//     #include <app/util/attribute-table.h>
// Due to their dependencies on zap-generated bits (even though most things should
// NOT depend on them)
//
// TODO: proper includes should be fixed
uint16_t emberAfIndexFromEndpointIncludingDisabledEndpoints(chip::EndpointId endpoint);
uint8_t emberAfClusterCount(chip::EndpointId endpoint, bool server);
const EmberAfCluster * emberAfGetNthCluster(chip::EndpointId endpoint, uint8_t n, bool server);
bool emberAfEndpointIndexIsEnabled(uint16_t index);
chip::EndpointId emberAfEndpointFromIndex(uint16_t index);
uint16_t emberAfEndpointCount();
uint8_t emberAfClusterCountByIndex(uint16_t endpointIndex, bool server);

// actual writes
EmberAfStatus emAfWriteAttribute(chip::EndpointId endpoint, chip::ClusterId cluster, chip::AttributeId attributeID, uint8_t * data,
                                 EmberAfAttributeType dataType, bool overrideReadOnlyAndDataType);

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

    // if the attribute is not inside the cluster, it may be a global one
    for (size_t i = 0; i < ArraySize(app::GlobalAttributesNotInMetadata); i++)
    {
        if (app::GlobalAttributesNotInMetadata[i] == path.GetAttribute().Raw())
        {
            return Attribute::IndexPath(cluster_index, Attribute::Index(cluster->attributeCount + i));
        }
    }

    return Attribute::IndexPath::Invalid();
}

Endpoint::Id EmberDatabase::IdForPath(Endpoint::Index idx)
{
    VerifyOrReturnValue(idx.IsValid(), Endpoint::Id::Invalid());

    return Endpoint::Id(emberAfEndpointFromIndex(idx.Raw()));
}

Cluster::Path EmberDatabase::IdForPath(Cluster::IndexPath idx)
{
    Endpoint::Id endpoint_id = IdForPath(idx.GetEndpoint());
    VerifyOrReturnValue(endpoint_id.IsValid(), Cluster::Path::Invalid());

    const EmberAfCluster * cluster = emberAfGetNthCluster(endpoint_id.Raw(), idx.GetCluster().Raw(), /* server = */ true);
    VerifyOrReturnValue(cluster != nullptr, Cluster::Path::Invalid());

    return Cluster::Path(endpoint_id, Cluster::Id(cluster->clusterId));
}

Attribute::Path EmberDatabase::IdForPath(Attribute::IndexPath idx)
{
    Endpoint::Id endpoint_id = IdForPath(idx.GetEndpoint());
    VerifyOrReturnValue(endpoint_id.IsValid(), Attribute::Path::Invalid());

    const EmberAfCluster * cluster = emberAfGetNthCluster(endpoint_id.Raw(), idx.GetCluster().Raw(), /* server = */ true);
    VerifyOrReturnValue(cluster != nullptr, Attribute::Path::Invalid());

    // Ordering for returns:
    //   - Cluster metadata attributes first
    //   - global attributes next
    if (idx.GetAttribute().Raw() < cluster->attributeCount)
    {
        return Attribute::Path(endpoint_id, Cluster::Id(cluster->clusterId),
                               Attribute::Id(cluster->attributes[idx.GetAttribute().Raw()].attributeId));
    }

    size_t globalIndex = idx.GetAttribute().Raw() - cluster->attributeCount;
    VerifyOrReturnValue(globalIndex < ArraySize(app::GlobalAttributesNotInMetadata), Attribute::Path::Invalid());

    return Attribute::Path(endpoint_id, Cluster::Id(cluster->clusterId),
                           Attribute::Id(app::GlobalAttributesNotInMetadata[globalIndex]));
}

Endpoint::Index EmberDatabase::EndpointEnd()
{
    return Endpoint::Index(emberAfEndpointCount());
}

Cluster::Index EmberDatabase::ClusterEnd(Endpoint::Index idx)
{
    VerifyOrReturnValue(idx.IsValid(), Cluster::Index(0));

    uint8_t count = emberAfClusterCountByIndex(idx.Raw(), /* server = */ true);
    return Cluster::Index(count);
}

Attribute::Index EmberDatabase::AttributeEnd(Cluster::IndexPath idx)
{
    Endpoint::Id endpoint_id = IdForPath(idx.GetEndpoint());
    VerifyOrReturnValue(endpoint_id.IsValid(), Attribute::Index(0));

    const EmberAfCluster * cluster = emberAfGetNthCluster(endpoint_id.Raw(), idx.GetCluster().Raw(), /* server = */ true);
    VerifyOrReturnValue(cluster != nullptr, Attribute::Index(0));

    return Attribute::Index(cluster->attributeCount + ArraySize(app::GlobalAttributesNotInMetadata));
}

bool EmberDatabase::IsEnabled(Endpoint::Id id)
{
    VerifyOrReturnValue(id.IsValid(), false);
    return IsEnabled(IndexOf(id));
}

bool EmberDatabase::IsEnabled(Endpoint::Index idx)
{
    VerifyOrReturnValue(idx.IsValid(), false);
    return emberAfEndpointIndexIsEnabled(idx.Raw());
}

CHIP_ERROR EmberDatabase::Read(Attribute::Path path, MutableByteSpan & data, DataType & type)
{
    VerifyOrReturnError(path.IsValid(), CHIP_ERROR_INVALID_ARGUMENT);

    // TODO
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

CHIP_ERROR EmberDatabase::Write(Attribute::Path path, WriteType write_type, MutableByteSpan data, DataType type)
{
    VerifyOrReturnError(path.IsValid(), CHIP_ERROR_INVALID_ARGUMENT);

    // TODO: data.size() is never used, so we do not seem to validate data
    EmberAfStatus ember_status = emAfWriteAttribute(                             //
        path.GetEndpoint().Raw(),                                                //
        path.GetCluster().Raw(),                                                 //
        path.GetAttribute().Raw(),                                               //
        data.data(),                                                             //
        to_underlying(type),                                                     //
        write_type == WriteType::kNoValidation /* overrideReadOnlyAndDataType */ //
    );

    Protocols::InteractionModel::Status im_status = app::ToInteractionModelStatus(ember_status);
    if (im_status == Protocols::InteractionModel::Status::Success)
    {
        return CHIP_NO_ERROR;
    }

#if CHIP_CONFIG_ERROR_SOURCE
    return ChipError(ChipError::SdkPart::kIMGlobalStatus, to_underlying(im_status), __FILE__, __LINE__);
#else
    return ChipError(ChipError::SdkPart::kIMGlobalStatus, to_underlying(im_status));
#endif
}

} // namespace Attributes
} // namespace chip
