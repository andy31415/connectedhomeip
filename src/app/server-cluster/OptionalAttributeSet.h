/*
 *    Copyright (c) 2025 Project CHIP Authors
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
#include <lib/core/DataModelTypes.h>
#include <lib/support/ReadOnlyBuffer.h>
#include <lib/support/Span.h>

namespace chip {
namespace app {
namespace Internal {

// Compile-time helper to check if an ID is in a list of IDs.
template <AttributeId T, AttributeId... Ts>
struct IsOneOf;

template <AttributeId T, AttributeId Head, AttributeId... Tail>
struct IsOneOf<T, Head, Tail...>
{
    static constexpr bool value = (T == Head) || IsOneOf<T, Tail...>::value;
};

template <AttributeId T>
struct IsOneOf<T>
{
    static constexpr bool value = false;
};

} // namespace Internal

/// It is very common that a class has optional attributes. Such optional attributes
/// need checking for and also affect what attributes are being returned by
/// server cluster implementations
///
/// This clas is defined to work only for at most 32 attributes as it uses an internal
/// 32-bit set to flag attrbutes as enabled or not.
class AttributeSet
{
public:
    AttributeSet()                                       = default;
    AttributeSet(AttributeSet && other)                  = default;
    AttributeSet & operator=(AttributeSet && other)      = default;
    AttributeSet(const AttributeSet & other)             = default;
    AttributeSet & operator=(const AttributeSet & other) = default;

    AttributeSet(Span<const DataModel::AttributeEntry> supportedAttributes) : mSupportedAttributes(supportedAttributes) {}

    // Checks if an attribute ID is set.
    bool IsSet(AttributeId id) const
    {
        const unsigned index = GetIndex(id);
        return (index != kInvalidIndex) && ((mSetBits & (1u << index)) != 0);
    }

    constexpr bool empty() const { return mSetBits == 0; }

    /// Append all enabled attributes to the given builder
    CHIP_ERROR AppendEnabled(ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder) const;

    /// Sets the given attribute enabled status.
    ///
    /// MUST be called for valid attribute IDs for this set only. If an invalid ID
    /// is passed in, no changes to the set occur.
    AttributeSet & Set(AttributeId id, bool value = true)
    {
        const unsigned index = GetIndex(id);
        if (index != kInvalidIndex)
        {
            if (value)
            {
                mSetBits |= static_cast<uint32_t>(1u << index);
            }
            else
            {
                mSetBits &= ~static_cast<uint32_t>(1u << index);
            }
        }
        return *this;
    }

private:
    static constexpr unsigned kInvalidIndex = 32;

    // Set bits according to the supported attributes.
    // Specifically every bit here corresponds to the underlying span
    uint32_t mSetBits = 0;
    Span<const DataModel::AttributeEntry> mSupportedAttributes;

    unsigned GetIndex(AttributeId id) const;
};

} // namespace app
} // namespace chip
