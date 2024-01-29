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

#include <attributes/typesafe.h>

namespace chip {
namespace Attributes {

/// Represents a path to a cluster, by IDs
class ClusterPath {
public:
    ClusterPath(Endpoint::Id endpoint,
                Cluster::Id cluster) : mEndpoint(endpoint), mCluster(cluster){}

    Endpoint::Id GetEndpoint() const { return mEndpoint; }
    Cluster::Id GetCluster() const { return mCluster; }
private:
    Endpoint::Id mEndpoint;
    Cluster::Id mCluster;
}


} // namespace Attributes
} // namespace chip
