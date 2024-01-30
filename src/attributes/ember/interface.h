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

#include <attributes/database/interface.h>

namespace chip {
namespace Attributes {

/// Forwards all operations from the database interface into
/// ember processing functions
class EmberDatabase : public Database
{

public:
    virtual ~Database() = default;

    Endpoint::Index IndexOf(Endpoint::Id) override;
    Cluster::IndexPath IndexOf(Cluster::Path) override;
    Attribute::IndexPath IndexOf(Attribute::Path) override;

    Endpoint::Id IdForPath(Endpoint::Index) override;
    Cluster::Path IdForPath(Cluster::IndexPath) override;
    Attribute::Path IdForPath(Attribute::IndexPath) override;

    Endpoint::Index EndpointEnd() override;
    Cluster::Index ClusterEnd(Endpoint::Index) override;
    Attribute::Index AttributeEnd(Cluster::IndexPath) override;

    bool IsEnabled(Endpoint::Id) override;
    bool IsEnabled(Cluster::Path) override;

    bool IsEnabled(Endpoint::Index) override;
    bool IsEnabled(Cluster::IndexPath) override;

    CHIP_ERROR WriteAttribute(Attribute::IndexPath attribute, ByteSpan data, DataType type) override;
    CHIP_ERROR ReadAttribute(Attribute::IndexPath attribute, MutableByteSpan & data, DataType & type) override;
};

} // namespace Attributes
} // namespace chip
