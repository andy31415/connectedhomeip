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

struct AttributeMeta
{
    AttributeId id;
    BitFlags<DataModel::AttributeQualityFlags> qualities;
    std::underlying_type_t<AttributePrivilege> privileges;
};

struct CommandMeta
{
    CommandId id;
    BitFlags<DataModel::CommandQualityFlags> qualities;
    Access::Privilege invokePrivilege;
};

struct ClusterMeta
{
    Span<AttributeMeta> attributes;
    Span<CommandMeta> acceptedCommands;
    Span<CommandId> generatedCommands;
};

// FIXME: define some things here for cluster metadata definition
//
// Also see updates from https://github.com/project-chip/connectedhomeip/pull/36493
//
// Overall requirements:
//   Cluster Metadata:
//      - ARRAY of attributes: id, quality, readPrivilege, writePrivilege
//      - ARRAY of commands: id, quality, privilege
//      - ARRAY of generatedCommands: id
//
//   Cluster INSTANCES:
//      - dataVersion
//
//   Endpoint INSTANCES:
//      - ARRAY of device types: where are these definitions? (DeviceTypeEntry)
//      - ARRAY of semantic tags                              (TAGS)
//      - ARRAY of Client clusters                            (METADATA)
//      - ARRAY of Server clusters                            (ID only)
//      - COMPOSITION:
//          - parentId -> optional (supports invalid)
//          - composition pattern (should re-use this one once 36493 is defined)

} // namespace Metadata
} // namespace app
} // namespace chip
