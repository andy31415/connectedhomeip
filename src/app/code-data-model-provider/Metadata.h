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

#include <app/data-model-provider/MetadataTypes.h>

namespace chip {
namespace app {
namespace Metadata {

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
