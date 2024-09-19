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

#include <app/AttributeValueDecoder.h>
#include <app/AttributeValueEncoder.h>
#include <app/data-model-provider/ActionReturnStatus.h>
#include <app/data-model-provider/Context.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <app/data-model-provider/OperationTypes.h>
#include <type_traits>

namespace chip {
namespace app {
namespace DynamicDataModel {

// A lambda specifically for reading functions
class ReadLambda
{
private:
    static constexpr std::size_t kAlign = sizeof(void *);
    static constexpr std::size_t kSize  = 32;

public:
    // Use initialize instead of constructor because this class has to be trivial
    template <typename Lambda>
    constexpr static ReadLambda For(const Lambda & lambda)
    {
        ReadLambda result;
        // memcpy is used to move the lambda aroud, so it must be trivially copyable
        static_assert(std::is_trivially_copyable<Lambda>::value, "lambda must be trivially copyable");
        static_assert(sizeof(Lambda) <= kSize, "lambda too large");
        static_assert(kAlign % alignof(Lambda) == 0, "lambda align too large");

        // Implicit cast a capture-less lambda into a raw function pointer.
        result.mLambdaProxy = [](const LambdaStorage & body, const DataModel::InteractionModelContext & context,
                                 const DataModel::ReadAttributeRequest & request,
                                 AttributeValueEncoder & encoder) -> DataModel::ActionReturnStatus {
            return (*reinterpret_cast<const Lambda *>(&body))(context, request, encoder);
        };
        ::memcpy(&result.mLambdaBody, &lambda, sizeof(Lambda));

        return result;
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
    constexpr static WriteLambda For(const Lambda & lambda)
    {
        WriteLambda result;

        // memcpy is used to move the lambda around, so it must be trivially copyable
        static_assert(std::is_trivially_copyable<Lambda>::value, "lambda must be trivially copyable");
        static_assert(sizeof(Lambda) <= kSize, "lambda too large");
        static_assert(kAlign % alignof(Lambda) == 0, "lambda align too large");

        // Implicit cast a capture-less lambda into a raw function pointer.
        result.mLambdaProxy = [](const LambdaStorage & body, const DataModel::InteractionModelContext & context,
                                 const DataModel::WriteAttributeRequest & request,
                                 AttributeValueDecoder & decoder) -> DataModel::ActionReturnStatus {
            return (*reinterpret_cast<const Lambda *>(&body))(context, request, decoder);
        };
        ::memcpy(&result.mLambdaBody, &lambda, sizeof(Lambda));
        return result;
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

} // namespace DynamicDataModel
} // namespace app
} // namespace chip
