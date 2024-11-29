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

#include <access/Privilege.h>
#include <app/AttributeAccessInterface.h>
#include <app/CommandHandlerInterface.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/BitFlags.h>
#include <lib/support/Span.h>

#include <cstdint>
#include <optional>
#include <type_traits>

namespace chip {
namespace app {
namespace Metadata {

/// More compact representation of attribute access privileges
enum AttributePrivilege : uint8_t
{
    kNone = 0,

    // Privleges are defined in chip::Access::Privilege (Privilege.h)
    // and contain 6 constants INCLUDING unused ones (ProxyView)
    // We use 3 bits for each of them
    kRead_View       = 1,
    kRead_ProxyView  = 2,
    kRead_Operate    = 3,
    kRead_Manage     = 4,
    kRead_Administer = 5,

    // the write privileges are the same as read, except shifted
    kWrite_View       = (kRead_View << 3),
    kWrite_ProxyView  = (kRead_ProxyView << 3),
    kWrite_Operate    = (kRead_Operate << 3),
    kWrite_Manage     = (kRead_Manage << 3),
    kWrite_Administer = (kRead_Administer << 3),
};

std::optional<Access::Privilege> ReadPrivilege(std::underlying_type_t<AttributePrivilege> value);
std::optional<Access::Privilege> WritePrivilege(std::underlying_type_t<AttributePrivilege> value);

/// Represents information about a single attribute that is exposed by a cluster
///
/// Generally compact and sufficient information to contain both ID and create
/// a DataModel::AttributeInfo entry
struct AttributeMeta
{
    AttributeId id;
    BitFlags<DataModel::AttributeQualityFlags> qualities;
    std::underlying_type_t<AttributePrivilege> privileges;
};

/// Represents information about a command exposed by a cluster
///
/// Generally compact and sufficient information to contain both ID and create
/// a DataModel::CommandInfo entry
struct CommandMeta
{
    CommandId id;
    BitFlags<DataModel::CommandQualityFlags> qualities;
    Access::Privilege invokePrivilege;
};

/// Repesents metadata for an exposed clusters:
///   - what attributes are contained within this cluster
///   - what commands are accepted for processing
///   - what commands are generated as responses
///
/// Information here is intended to sufficiently cover all data returned
/// by per-cluster ProviderMetadatatre queries: first/next/get attribute,
/// first/next/get accepted command and first/next generated command
struct ClusterMeta
{
    Span<const AttributeMeta> attributes;
    Span<const CommandMeta> acceptedCommands;
    Span<const CommandId> generatedCommands;
};

/// Represents metadata for a specific INSTANTIATED cluster
///
/// A cluster generally contains metadata but also has a current 'version'
/// of attributes as well as processing logic.
struct ClusterInstance
{
    DataVersion dataVersion;
    const ClusterMeta * metadata;
    const AttributeAccessInterface * attributeHandler;
    const CommandHandlerInterface * commandHandler;
};

// FIXME: define some things here for cluster metadata definition
//
// Also see updates from https://github.com/project-chip/connectedhomeip/pull/36493
//
// Overall requirements:
//   [DONE] Cluster Metadata:
//      - ARRAY of attributes: id, quality, readPrivilege, writePrivilege
//      - ARRAY of commands: id, quality, privilege
//      - ARRAY of generatedCommands: id
//
//   [DONE] Cluster INSTANCES:
//      - dataVersion
//      - Cluster Metadata
//
//   [TODO] Endpoint INSTANCES:
//      - ARRAY of device types: where are these definitions? (DeviceTypeEntry)
//      - ARRAY of semantic tags                              (TAGS)
//      - ARRAY of Client clusters                            (CLUSTER INSTANCES)
//      - ARRAY of Server clusters                            (ID only)
//      - COMPOSITION:
//          - parentId -> optional (supports invalid)
//          - composition pattern (should re-use this one once 36493 is defined)

} // namespace Metadata
} // namespace app
} // namespace chip
