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

#include "lib/core/CHIPError.h"
#include <access/Privilege.h>
#include <app/AttributeValueDecoder.h>
#include <app/AttributeValueEncoder.h>
#include <app/CommandHandler.h>
#include <app/data-model-provider/ActionReturnStatus.h>
#include <app/data-model-provider/Context.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <app/data-model-provider/OperationTypes.h>
#include <app/data-model/FabricScoped.h>
#include <lib/support/BitFlags.h>

#include <array>
#include <cstdint>
#include <optional>
#include <type_traits>

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

// A lambda specifically for reading functions
class ReadLambda
{
private:
    static constexpr std::size_t kAlign = sizeof(void *);
    static constexpr std::size_t kSize  = 32;

public:
    // Use initialize instead of constructor because this class has to be trivial
    template <typename Lambda>
    void Initialize(const Lambda & lambda)
    {
        // memcpy is used to move the lambda into the event queue, so it must be trivially copyable
        static_assert(std::is_trivially_copyable<Lambda>::value, "lambda must be trivially copyable");
        static_assert(sizeof(Lambda) <= kSize, "lambda too large");
        static_assert(kAlign % alignof(Lambda) == 0, "lambda align too large");

        // Implicit cast a capture-less lambda into a raw function pointer.
        mLambdaProxy = [](const LambdaStorage & body, const DataModel::InteractionModelContext & context,
                          const DataModel::ReadAttributeRequest & request,
                          AttributeValueEncoder & encoder) -> DataModel::ActionReturnStatus {
            return (*reinterpret_cast<const Lambda *>(&body))(context, request, encoder);
        };
        ::memcpy(&mLambdaBody, &lambda, sizeof(Lambda));
    }

    DataModel::ActionReturnStatus operator()(const DataModel::InteractionModelContext & context,
                                             const DataModel::ReadAttributeRequest & request, AttributeValueEncoder & encoder) const
    {
        return mLambdaProxy(mLambdaBody, context, request, encoder);
    }

private:
    using LambdaStorage = std::aligned_storage_t<kSize, kAlign>;
    DataModel::ActionReturnStatus (*mLambdaProxy)(const LambdaStorage & body, const DataModel::InteractionModelContext & context,
                                                  const DataModel::ReadAttributeRequest & request, AttributeValueEncoder & encoder);
    LambdaStorage mLambdaBody;
};

// A lambda specifically for reading functions
class WriteLambda
{
private:
    static constexpr std::size_t kAlign = sizeof(void *);
    static constexpr std::size_t kSize  = 32;

public:
    // Use initialize instead of constructor because this class has to be trivial
    template <typename Lambda>
    void Initialize(const Lambda & lambda)
    {
        // memcpy is used to move the lambda into the event queue, so it must be trivially copyable
        static_assert(std::is_trivially_copyable<Lambda>::value, "lambda must be trivially copyable");
        static_assert(sizeof(Lambda) <= kSize, "lambda too large");
        static_assert(kAlign % alignof(Lambda) == 0, "lambda align too large");

        // Implicit cast a capture-less lambda into a raw function pointer.
        mLambdaProxy = [](const LambdaStorage & body, const DataModel::InteractionModelContext & context,
                          const DataModel::WriteAttributeRequest & request,
                          AttributeValueDecoder & decoder) -> DataModel::ActionReturnStatus {
            return (*reinterpret_cast<const Lambda *>(&body))(context, request, decoder);
        };
        ::memcpy(&mLambdaBody, &lambda, sizeof(Lambda));
    }

    DataModel::ActionReturnStatus operator()(const DataModel::InteractionModelContext & context,
                                             const DataModel::WriteAttributeRequest & request,
                                             AttributeValueDecoder & decoder) const
    {
        return mLambdaProxy(mLambdaBody, context, request, decoder);
    }

private:
    using LambdaStorage = std::aligned_storage_t<kSize, kAlign>;
    DataModel::ActionReturnStatus (*mLambdaProxy)(const LambdaStorage & body, const DataModel::InteractionModelContext & context,
                                                  const DataModel::WriteAttributeRequest & request,
                                                  AttributeValueDecoder & decoder);
    LambdaStorage mLambdaBody;
};

/// Represents the definition on how an attribute should be handled
///
/// Maintains attribute metadata  as well as read/write functions to use
/// when operating on such a function.
struct AttributeDefinition
{
    AttributeId id;
    DataModel::AttributeInfo metadata;
    std::optional<ReadLambda> readFunction   = std::nullopt;
    std::optional<WriteLambda> writeFunction = std::nullopt;

    constexpr AttributeDefinition(AttributeId attributeId) :
        id(attributeId), metadata({
                             BitFlags<DataModel::AttributeQualityFlags>(0) /* flags */,
                             std::make_optional(Access::Privilege::kView) /* readPrivilege */,
                             std::make_optional(Access::Privilege::kOperate) /* writePrivilege */,
                         })
    {}

    /// Set a flag on the attribute (like list, scoped, timed etc.)
    constexpr AttributeDefinition & AddFlag(DataModel::AttributeQualityFlags flag)
    {
        metadata.flags.Set(flag);
        return *this;
    }

    constexpr AttributeDefinition & SetReadPrivilege(Access::Privilege p)
    {
        metadata.readPrivilege = std::make_optional(p);
        return *this;
    }

    constexpr AttributeDefinition & SetWritePrivilege(Access::Privilege p)
    {
        metadata.writePrivilege = std::make_optional(p);
        return *this;
    }

    constexpr AttributeDefinition & SetReadFunction(const ReadLambda & f)
    {
        readFunction = std::make_optional(f);
        return *this;
    }

    constexpr AttributeDefinition & SetWriteFunction(const WriteLambda & f)
    {
        writeFunction = std::make_optional(f);
        return *this;
    }
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

    /// Methods to be used by subclasses when configuring read/writes
    template <typename Class, typename DataType>
    constexpr ReadLambda ReadVia(Class * object, DataType (Class::*memberFunction)())
    {
        ReadLambda value;
        value.Initialize([this, object, memberFunction](const DataModel::InteractionModelContext & context,
                                                        const DataModel::ReadAttributeRequest & request,
                                                        AttributeValueEncoder & encoder) -> DataModel::ActionReturnStatus {
            // memberFunction is a pure getter, so it never fails
            return encoder.Encode((object->*memberFunction)());
        });
        return value;
    }

    template <typename Class, typename DataType>
    constexpr WriteLambda WriteVia(Class * object, void (Class::*memberFunction)(DataType value))
    {
        WriteLambda value;
        value.Initialize([this, object, memberFunction](const DataModel::InteractionModelContext & context,
                                                        const DataModel::WriteAttributeRequest & request,
                                                        AttributeValueDecoder & decoder) -> DataModel::ActionReturnStatus {
            DataType data;
            ReturnErrorOnFailure(decoder.Decode(data));
            (object->*memberFunction)(data);
            return CHIP_NO_ERROR;
        });
        return value;
    }

    template <typename Class, typename DataType>
    constexpr WriteLambda WriteVia(Class * object, CHIP_ERROR (Class::*memberFunction)(DataType value))
    {
        WriteLambda value;
        value.Initialize([this, object, memberFunction](const DataModel::InteractionModelContext & context,
                                                        const DataModel::WriteAttributeRequest & request,
                                                        AttributeValueDecoder & decoder) -> DataModel::ActionReturnStatus {
            std::decay_t<DataType> data;
            ReturnErrorOnFailure(decoder.Decode(data));
            return (object->*memberFunction)(data);
        });
        return value;
    }

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
    [[nodiscard]] virtual const AttributeDefinition * AttributesBegin() const = 0;
    [[nodiscard]] virtual const AttributeDefinition * AttributesEnd() const   = 0;
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

    [[nodiscard]] const AttributeDefinition * AttributesBegin() const override { return mAttributes.cbegin(); }
    [[nodiscard]] const AttributeDefinition * AttributesEnd() const override { return mAttributes.cend(); }
};

} // namespace DynamicDataModel
} // namespace app
} // namespace chip
