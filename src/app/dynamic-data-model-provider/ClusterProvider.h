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
//
// Need to solve iterator problem first.
//
// Problem statement:
//   - need a reasonably fast way for: FirstID (trivial) and NextID(previous)
//   - Ideally this is an iterator encapsulation HOWEVER that requires storage knowledge and breaks
//     across interfaces
//   - we can provide some context intptr_t for example to free up things. This seems equivalent to
//     allowing a single context:
//       - CANNOT be a real pointer as there is no "free". Suggesting to just use some context.

#include <app/AttributeValueDecoder.h>
#include <app/AttributeValueEncoder.h>
#include <app/CommandHandler.h>
#include <app/data-model-provider/ActionReturnStatus.h>
#include <app/data-model-provider/Context.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <app/data-model-provider/OperationTypes.h>

#include <cstdint>
#include <optional>

// General interface expectation that would allow some internal hint in the caller class
template <typename T>
T NextItem(const T & previous, uintptr_t & hint);

namespace chip {
namespace app {
namespace DynamicDataModel {

class Cluster
{
public:
    /// TODO: figure out a way to pass in `self` as callbacks to read/write attributes
    ///       we may want to use lambdabridge in some way (do we have enough space???)


    // TODO: cluster metadata:
    //   - BitFlags<ClusterQualityFlags>
    //   - data version <- this MUST be modifiable because of self

    // TODO: add iteration (and exitence? we have Get capability here... for commands)
    // 
    // we need from MetadataTypes.h/ProviderMetadataTree:
    //   - attributes: first, next, get
    //   - commands: first, next, get
    //   - generated commands: first, next
    //
    //   Pattern to use: use uintptr_t as a "hint" (this can be used in the actual provider as well
    //   and clusters should probably be grouped within a endpoint for composition purposes)

    // Generic attribute operations below. Their implementation redirects to the underlying cluster
    // data definitions.

    /// Performs a read on the given attribute.
    ///
    /// request.path MUST correspond to the current cluster id
    DataModel::ActionReturnStatus ReadAttribute(const DataModel::InteractionModelContext & context,
                                                const DataModel::ReadAttributeRequest & request, AttributeValueEncoder & encoder);

    /// Performs a write on the given attribute.
    ///
    /// request.path MUST correspond to the current cluster id
    DataModel::ActionReturnStatus WriteAttribute(const DataModel::InteractionModelContext & context,
                                                 const DataModel::WriteAttributeRequest & request, AttributeValueDecoder & decoder);


    /// Performs an invoke for the given command
    ///
    /// request.path MUST correspond to the current cluster id
    std::optional<DataModel::ActionReturnStatus> Invoke(const DataModel::InteractionModelContext & context,
                                                        const DataModel::InvokeRequest & request,
                                                        chip::TLV::TLVReader & input_arguments, CommandHandler * handler);
private:
    // TODO: need list of attributes
    // TODO: need list of commands
};

} // namespace DynamicDataModel
} // namespace app
} // namespace chip
