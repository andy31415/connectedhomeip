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
///
/// The implementation of the class generally is a wrapper over a bitset with a
/// `IsSet()` method. Configurations should use the OptionalAttributeSet<...> class.
class AttributeSet
{
public:
    AttributeSet()                                       = default;
    AttributeSet(const AttributeSet & other)             = default;
    AttributeSet(AttributeSet && other)                  = default;
    AttributeSet & operator=(const AttributeSet & other) = default;
    AttributeSet & operator=(AttributeSet && other)      = default;

    AttributeSet(Span<const DataModel::AttributeEntry> supportedAttributes) : mSupportedAttributes(supportedAttributes) {}

    // Checks if an attribute ID is set.
    constexpr bool IsSet(AttributeId id) const { return (mSetBits & (1u << GetIndex(id))) != 0; }

    constexpr bool empty() const { return mSetBits == 0; }

    /// Append all enabled attributes to the given builder
    CHIP_ERROR AppendEnabled(ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder) const;

    /// Exposes a "force attribute bit set" without extra validation,
    /// so that clusters can enforce specific bits to be set.
    ///
    /// This is NOT intended as a generic set, use `OptionalAttributeSet` to configure values.
    template <AttributeId id>
    constexpr AttributeSet & ForceSet()
    {
        static_assert(id < 32, "Attribute ID must be settable");
        return Set(id);
    }

protected:
    constexpr AttributeSet & Set(AttributeId id)
    {
        mSetBits |= static_cast<uint32_t>((static_cast<uint32_t>(1) << GetIndex(id)));
        return *this;
    }

private:
    // Set bits according to the supported attributes.
    // Specifically every bit here corresponds to the underlying span
    uint32_t mSetBits = 0;
    Span<const DataModel::AttributeEntry> mSupportedAttributes;

    constexpr unsigned GetIndex(AttributeId id) const
    {
        unsigned idx = 0;
        for (const auto entry : mSupportedAttributes)
        {
            if (entry.attributeId == id)
            {
                return idx;
            }
            idx++;
        }
        // should NEVER happen
        chipDie();
    }
};

/// A specialization of AttributeSet that provides checked calls to `Set`.
///
/// Specifically it requires that attributes are declared as part of the template
/// parameter pack.
///
/// NOTE: this will NOT work for all possible attributes/clusters, only for clusters
///       whose optional attributes all have IDs under 32. Static asserts are in place
///       to ensure that arguments to the template are valid.
///
/// Example usage:
///
///    namespace chip::app::Clusters::GeneralDiagnostics {
///
///    using OptionalAttributeSet = chip::app::OptionalAttributeSet<
///        Attributes::TotalOperationalHours::Id,
///        Attributes::BootReason::Id,
///        Attributes::ActiveHardwareFaults::Id
///    >;
///
///    } // namespace chip::app::Clusters::GeneralDiagnostics
///
/// After this, one can:
///
///   GeneralDiagnostics::OptionalAttributeSet()
///      .Set<GeneralDiagnostics::Attributes::TotalOperationalHours::Id>()
///      .Set<GeneralDiagnostics::Attributes::BootReason::Id>();
///
/// Cluster implementations can then store a
///   Constructor(const GeneralDiagnostics::OptionalAttributeSet& optionalAttributeSet) :
///   mOptionalAttributeSet(optionalAttributeSet) {...}
///
/// where:
///   const AttributeSet mOptionalAttributeSet;
template <const DataModel::AttributeEntry &... OptionalAttributeEntries>
class OptionalAttributeSet : public AttributeSet
{
private:
    // To be able to Span over the list of attributes, we need to have them in an array.
    static constexpr DataModel::AttributeEntry kOptionalAttributes[] = { OptionalAttributeEntries... };

public:
    OptionalAttributeSet() : AttributeSet(Span(kOptionalAttributes)) {}

    template <uint32_t ATTRIBUTE_ID>
    constexpr OptionalAttributeSet & Set(bool value = true)
    {
        static_assert(((ATTRIBUTE_ID == OptionalAttributeEntries.attributeId) || ...), "attribute MUST be optional");
        if (value)
        {
            (void) AttributeSet::Set(ATTRIBUTE_ID);
        }
        return *this;
    }
};

} // namespace app
} // namespace chip
