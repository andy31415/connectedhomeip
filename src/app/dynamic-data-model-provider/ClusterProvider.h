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

// FIXME: implement
//
// TODO:
//   - definitions in MetadataTypes are useful. Every of attribute/command expose
//     an ID and a Attribute/CommandInfo to be fully defined
//   - Expectations:
//      - Cluster global information:
//         - data version
//         - qualities
//      - MUST be capable to update own version
//      - Can list:
//        - attributes
//        - accepted commands
//        - generated commands (huge annoyance!)
//      - provides:
//        - Read, Write, Invoke
//
// Implementation details:      
//   - List of attributes
//     - general storage is {id, info, reader, writer (this affects ability to read/write)}
//   - List of commands
//     - general storage is {id, info, handler}
//   - GeneratedCommands (how? We can implement a slow version of this ...)
//     - general storage is a list
//     - have helper of iterator from a array (which can be initialized)
// 
// Problem on iterator-based implementation:
//   - O(n^2) if we iterate from the start
//   - Interface on main provider is using a next(previous) implementation which
//     is far from ideal
