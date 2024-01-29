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

#include <stddef.h>

#include <lib/core/DataModelTypes.h>

namespace chip {
namespace Attributes {

namespace Impl {

/// Wraps a specific type to force type consistency
template <typename T>
class RawWrapper
{
public:
    explicit RawWrapper(T t) : mValue(t) {}
    T Raw() const { return mValue; }

    bool operator==(const RawWrapper & other) const { return mValue == other.mValue; }

private:
    const T mValue;
};

} // namespace Impl

#define TYPESAFE_WRAP(_NAME, _TYPE)                                                                                                \
    struct _NAME : public chip::Attributes::Impl::RawWrapper<_TYPE>     \
    {                                                                                                                              \
        explicit _NAME(_TYPE _x) : chip::Attributes::Impl::RawWrapper<_TYPE>(_x) {} \
    }

// `Id` and `Index` are typesafe wrappers around the integer types of
// id and index. This is to prevent mixing of various ids and indices at compile time.
namespace Endpoint {
TYPESAFE_WRAP(Id, chip::EndpointId);
TYPESAFE_WRAP(Index, size_t);
} // namespace Endpoint

namespace Cluster {
TYPESAFE_WRAP(Id, chip::ClusterId);
TYPESAFE_WRAP(Index, size_t);
} // namespace Cluster

namespace Attribute {
TYPESAFE_WRAP(Id, chip::AttributeId);
TYPESAFE_WRAP(Index, size_t);
} // namespace Cluster

} // namespace Attributes
} // namespace chip
