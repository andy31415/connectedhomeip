/*
 *
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

#include <limits>
#include <stddef.h>

#include <lib/core/DataModelTypes.h>

namespace chip {
namespace Attributes {

static constexpr size_t kInvalidIndexValue = std::numeric_limits<size_t>::max();

// General rules for Id and Index types:
//
// - Only assign and compare of the same type is allowed.
//   I.e. Endpoint::Id is NOT compatible Attribute::Id
//
// - ::Id - COMPARE (==, !=)
//
// - ::Index - COMPARE (==, !=), ORDER (<,>,<=, >=),
//             INCREMENT (++), DECREMENT (--)

namespace Impl {

/// Wraps a specific type to force type consistency
///
/// Comparison methods are protected so only subclasses can use them, to explicitly deny
/// comparisons with incompatible types.
template <typename T>
class RawWrapper
{
public:
    explicit RawWrapper(T t) : mValue(t) {}
    T Raw() const { return mValue; }

protected:
    bool eq(const RawWrapper & other) const { return mValue == other.mValue; }
    bool ne(const RawWrapper & other) const { return mValue != other.mValue; }

    T mValue;
};

// A wrapper around an index. Adds ability to compare and increment as those
// have meaning for indices
///
/// Comparison methods are protected so only subclasses can use them, to explicitly deny
/// comparisons with incompatible types.
struct RawIndex : public RawWrapper<size_t>
{
    explicit RawIndex(size_t t) : RawWrapper<size_t>(t) {}

    bool IsValid() const { return Raw() != kInvalidIndexValue; }

    RawIndex & operator++()
    {
        ++mValue;
        return *this;
    }

    RawIndex & operator--()
    {
        --mValue;
        return *this;
    }

    RawIndex operator++(int)
    {
        RawIndex temp = *this;
        ++*this;
        return temp;
    }

    RawIndex operator--(int)
    {
        RawIndex temp = *this;
        --*this;
        return temp;
    }

protected:
    bool lt(const RawIndex & other) const { return Raw() < other.Raw(); }
    bool le(const RawIndex & other) const { return Raw() <= other.Raw(); }
    bool gt(const RawIndex & other) const { return Raw() > other.Raw(); }
    bool ge(const RawIndex & other) const { return Raw() >= other.Raw(); }
};

} // namespace Impl

#define TYPESAFE_WRAP(_NAME, _TYPE, _INVALID_VALUE)                                                                                \
    struct _NAME : public chip::Attributes::Impl::RawWrapper<const _TYPE>                                                          \
    {                                                                                                                              \
        using Base = chip::Attributes::Impl::RawWrapper<const _TYPE>;                                                              \
                                                                                                                                   \
        _NAME() : Base(_INVALID_VALUE) {}                                                                                          \
        explicit _NAME(_TYPE _x) : Base(_x) {}                                                                                     \
        bool operator==(const _NAME & other) const                                                                                 \
        {                                                                                                                          \
            return eq(other);                                                                                                      \
        }                                                                                                                          \
        bool operator!=(const _NAME & other) const                                                                                 \
        {                                                                                                                          \
            return ne(other);                                                                                                      \
        }                                                                                                                          \
    } // namespace Attributes

// Note: comparison operations are in here on purpose, to not allow
//       comparing of different index types
#define TYPESAFE_WRAP_INDEX(_NAME)                                                                                                 \
    struct _NAME : public chip::Attributes::Impl::RawIndex                                                                         \
    {                                                                                                                              \
        explicit _NAME() : chip::Attributes::Impl::RawIndex(0) {}                                                                  \
        explicit _NAME(size_t _x) : chip::Attributes::Impl::RawIndex(_x) {}                                                        \
                                                                                                                                   \
        static _NAME Invalid()                                                                                                     \
        {                                                                                                                          \
            return _NAME(kInvalidIndexValue);                                                                                      \
        }                                                                                                                          \
                                                                                                                                   \
        bool operator==(const _NAME & other) const                                                                                 \
        {                                                                                                                          \
            return eq(other);                                                                                                      \
        }                                                                                                                          \
        bool operator!=(const _NAME & other) const                                                                                 \
        {                                                                                                                          \
            return ne(other);                                                                                                      \
        }                                                                                                                          \
        bool operator<(const _NAME & other) const                                                                                  \
        {                                                                                                                          \
            return lt(other);                                                                                                      \
        }                                                                                                                          \
        bool operator<=(const _NAME & other) const                                                                                 \
        {                                                                                                                          \
            return le(other);                                                                                                      \
        }                                                                                                                          \
        bool operator>(const _NAME & other) const                                                                                  \
        {                                                                                                                          \
            return gt(other);                                                                                                      \
        }                                                                                                                          \
        bool operator>=(const _NAME & other) const                                                                                 \
        {                                                                                                                          \
            return ge(other);                                                                                                      \
        }                                                                                                                          \
    }

// `Id` and `Index` are typesafe wrappers around the integer types of
// id and index. This is to prevent mixing of various ids and indices at compile time.
namespace Endpoint {
TYPESAFE_WRAP(Id, chip::EndpointId, chip::kInvalidEndpointId);
TYPESAFE_WRAP_INDEX(Index);
} // namespace Endpoint

namespace Cluster {
TYPESAFE_WRAP(Id, chip::ClusterId, chip::kInvalidClusterId);
TYPESAFE_WRAP_INDEX(Index);
} // namespace Cluster

namespace Attribute {
TYPESAFE_WRAP(Id, chip::AttributeId, chip::kInvalidAttributeId);
TYPESAFE_WRAP_INDEX(Index);
} // namespace Attribute

} // namespace Attributes
} // namespace chip
