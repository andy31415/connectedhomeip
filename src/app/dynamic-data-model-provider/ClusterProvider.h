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
#include <app/CommandHandler.h>
#include <app/ConcreteAttributePath.h>
#include <app/data-model-provider/ActionReturnStatus.h>
#include <app/data-model-provider/Context.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <app/data-model-provider/OperationTypes.h>
#include <app/data-model/FabricScoped.h>
#include <app/dynamic-data-model-provider/AttributeDefinition.h>
#include <lib/core/CHIPError.h>
#include <lib/support/BitFlags.h>

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

/// Defines a cluster implementation that is able to handle a set of attributes and commands.
///
/// ClusterBase objects are capable to handle read/write/invoke and provide
/// sufficient metadata to fully support iteration as DataModel::ProviderMetadataTree for
/// an individual cluster.
///
/// Objects of this type are not often created as a stand-alone and are rather created
/// out of `Cluster`
///
class ClusterBase
{
public:
    virtual ~ClusterBase() = default;

    /// Methods to be used by subclasses when configuring read/writes
    template <typename Class, typename DataType>
    constexpr ReadLambda ReadVia(Class * object, DataType (Class::*memberFunction)())
    {
        return ReadLambda::For([object, memberFunction](const DataModel::InteractionModelContext & context,
                                                        const DataModel::ReadAttributeRequest & request,
                                                        AttributeValueEncoder & encoder) -> DataModel::ActionReturnStatus {
            // memberFunction is a pure getter, so it never fails
            return encoder.Encode((object->*memberFunction)());
        });
    }

    /// TODO: methods below are UNABLE to verify if "data changed" or not and call appropriate values.
    ///       We should have a way to signal "value unchanged" vs "value changed" successes here as well as
    ///       the whole assortment of possible failures.

    template <typename Class, typename DataType>
    constexpr WriteLambda WriteVia(Class * object, void (Class::*memberFunction)(DataType value))
    {
        return WriteLambda::For([object, memberFunction](const DataModel::InteractionModelContext & context,
                                                         const DataModel::WriteAttributeRequest & request,
                                                         AttributeValueDecoder & decoder) -> DataModel::ActionReturnStatus {
            DataType data;
            ReturnErrorOnFailure(decoder.Decode(data));
            // memberFunction is a pure setter, so it never fails
            (object->*memberFunction)(data);
            return CHIP_NO_ERROR;
        });
    }

    template <typename Class, typename DataType>
    constexpr WriteLambda WriteVia(Class * object, CHIP_ERROR (Class::*memberFunction)(DataType value))
    {
        return WriteLambda::For([object, memberFunction](const DataModel::InteractionModelContext & context,
                                                         const DataModel::WriteAttributeRequest & request,
                                                         AttributeValueDecoder & decoder) -> DataModel::ActionReturnStatus {
            std::decay_t<DataType> data;
            ReturnErrorOnFailure(decoder.Decode(data));
            return (object->*memberFunction)(data);
        });
    }

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
    
    /// Get a snapshot of the current cluster information.
    DataModel::ClusterInfo GetClusterInfo() const
    {
        DataModel::ClusterInfo result(mDataVersion);
        result.flags = GetClusterFlags();
        return result;
    }

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
    [[nodiscard]] virtual const AttributeDefinition * AttributesBegin() const = 0;
    [[nodiscard]] virtual const AttributeDefinition * AttributesEnd() const   = 0;

    [[nodiscard]] const AttributeDefinition * AttributeDefinitionForPath(const ConcreteAttributePath & path) const;

    [[nodiscard]] virtual BitFlags<DataModel::ClusterQualityFlags> GetClusterFlags() const
    {
        return BitFlags<DataModel::ClusterQualityFlags>();
    }

private:
    DataVersion mDataVersion = 0; // TODO: cluster version SHOULD/COULD be random
};

template <size_t kAttributeCount = 0>
class Cluster : public ClusterBase
{
public:
    using AttributeArray = std::array<AttributeDefinition, kAttributeCount>;

    constexpr Cluster(const AttributeArray & attributes) : mAttributes(attributes) {}
    ~Cluster() override = default;

protected:
    const AttributeArray mAttributes;

    [[nodiscard]] const AttributeDefinition * AttributesBegin() const override { return mAttributes.data(); }
    [[nodiscard]] const AttributeDefinition * AttributesEnd() const override { return mAttributes.data() + kAttributeCount; }
};

} // namespace DynamicDataModel
} // namespace app
} // namespace chip
