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

#include "access/Privilege.h"
#include "lib/support/BitFlags.h"
#include "lib/support/LambdaBridge.h"
#include <app/AttributeValueDecoder.h>
#include <app/AttributeValueEncoder.h>
#include <app/CommandHandler.h>
#include <app/data-model-provider/ActionReturnStatus.h>
#include <app/data-model-provider/Context.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <app/data-model-provider/OperationTypes.h>

#include <array>
#include <cstdint>
#include <optional>

// General interface expectation that would allow some internal hint in the caller class
template <typename T>
T NextItem(const T & previous, uintptr_t & hint);

namespace chip {
namespace app {
namespace DynamicDataModel {

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

/// Represents the definition on how an attribute should be handled
///
/// For a given attribute, one has the capability to:
struct AttributeDefinition
{
    AttributeId id;
    DataModel::AttributeInfo metadata;
    std::optional<LambdaBridge> readFunction;
    std::optional<LambdaBridge> writeFunction;
};

class AttributeDefinitionBuilder
{
public:
    constexpr AttributeDefinitionBuilder(AttributeId id) :
        mDefinition{
            id,
            {
                BitFlags<DataModel::AttributeQualityFlags>(0) /* flags */,
                std::make_optional(Access::Privilege::kView) /* readPrivilege */,
                std::make_optional(Access::Privilege::kOperate) /* writePrivilege */,
            },
            std::nullopt /* readFunction */,
            std::nullopt /* writeFunction */,
        }
    {}

    /// Set a flag on the attribute (like list, scoped, timed etc.)
    constexpr AttributeDefinitionBuilder & AddFlag(DataModel::AttributeQualityFlags flag)
    {
        mDefinition.metadata.flags.Set(flag);
        return *this;
    }

    constexpr AttributeDefinitionBuilder & SetReadPrivilege(Access::Privilege p)
    {
        mDefinition.metadata.readPrivilege = std::make_optional(p);
        return *this;
    }

    constexpr AttributeDefinitionBuilder & SetWritePrivilege(Access::Privilege p)
    {
        mDefinition.metadata.writePrivilege = std::make_optional(p);
        return *this;
    }

    constexpr AttributeDefinitionBuilder & SetReadFunction(const LambdaBridge & f)
    {
        mDefinition.readFunction = std::make_optional(f);
        return *this;
    }

    constexpr AttributeDefinitionBuilder & SetWriteFunction(const LambdaBridge & f)
    {
        mDefinition.writeFunction = std::make_optional(f);
        return *this;
    }

    [[nodiscard]] constexpr AttributeDefinition Build() const { return mDefinition; }

private:
    AttributeDefinition mDefinition;
};

/// Defines a cluster implementation that is able to handle a set of attributes and commands
///
/// TODO: how do I generate events ?
///
/// TODO:
///    - attribute storage -> This includes read/write invocations
///    - command storage -> This includes invoke calls
///
/// Want: cluster class to be well defined, so need subclasses to provide the data storage
///       for the underlying storage ...
class ClusterBase
{
public:
    virtual ~ClusterBase() = default;

    // TODO: figure out a way to pass in `self` as callbacks to read/write attributes
    //       we may want to use lambdabridge in some way (do we have enough space???)

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

protected:
    virtual AttributeDefinition * AttributesBegin() = 0;
    virtual AttributeDefinition * AttributesEnd()   = 0;
};

template <size_t kAttributeCount = 0>
class Cluster : ClusterBase
{
public:
    using AttributeArray = std::array<AttributeDefinition, kAttributeCount>;

    constexpr Cluster(const AttributeArray & attributes) : mAttributes(attributes) {}
    ~Cluster() override = default;

protected:
    AttributeArray mAttributes;

    AttributeDefinition * AttributesBegin() override { return mAttributes.begin(); }
    AttributeDefinition * AttributesEnd() override { return mAttributes.end(); }
};

} // namespace DynamicDataModel
} // namespace app
} // namespace chip
