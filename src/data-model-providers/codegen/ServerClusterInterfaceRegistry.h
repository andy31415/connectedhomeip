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

#include <app/ConcreteClusterPath.h>
#include <app/server-cluster/ServerClusterInterface.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>

namespace chip {
namespace app {

/// Allows registering and retrieving ServerClusterInterface instances for specific cluster paths.
class ServerClusterInterfaceRegistry
{
private:
    struct RegisteredServerClusterInterface;

public:
    /// represents an iterable list of clusters
    class ClustersList
    {
    public:
        class Iterator
        {
        public:
            Iterator(RegisteredServerClusterInterface * interface) : mInterface(interface) {}

            Iterator & operator++()
            {
                if (mInterface != nullptr)
                {
                    mInterface = mInterface->next;
                }
                return *this;
            }
            bool operator==(const Iterator & other) const { return mInterface == other.mInterface; }
            bool operator!=(const Iterator & other) const { return mInterface != other.mInterface; }
            ServerClusterInterface * operator*() { return mInterface->serverClusterInterface; }

        private:
            RegisteredServerClusterInterface * mInterface;
        };

        ClustersList(RegisteredServerClusterInterface * start) : mStart(start) {}
        Iterator begin() { return mStart; }
        Iterator end() { return nullptr; }

    private:
        RegisteredServerClusterInterface * mStart;
    };

    ~ServerClusterInterfaceRegistry();

    /// Associate a specific interface with the given endpoint.
    ///
    /// There can be only a single registration for a given `endpointId/clusterId` path.
    /// A registration will return an error if a registration already exists on
    /// the given `endpointId/cluster->GetClusterID()` already exists
    ///
    /// Registrations need a valid `endpointId` and `cluster->GetClusterID()` MUST be a valid cluster id.
    [[nodiscard]] CHIP_ERROR Register(EndpointId endpointId, ServerClusterInterface * cluster);

    /// Remove an existing registration for a given endpoint/cluster path.
    ///
    /// Returns the previous registration if any exists (or nullptr if nothing
    /// to unregister)
    ServerClusterInterface * Unregister(const ConcreteClusterPath & path);

    /// Return the interface registered for the given cluster path or nullptr if one does not exist
    ServerClusterInterface * Get(const ConcreteClusterPath & path);

    /// Provides a list of clusters that are registered for the given endpoint.
    ///
    /// As ClustersList points inside the internal registrations of the registry,
    /// the list is only valid as long as the registry is not modified.
    ClustersList ClustersOnEndpoint(EndpointId endpointId);

    /// Unregister all registrations for the given endpoint.
    void UnregisterAllFromEndpoint(EndpointId endpointId);

private:
    /// A single-linked list of registered server cluster interfaces
    struct RegisteredServerClusterInterface
    {
        // A single-linked list of clusters registered for the given `endpointId`
        ServerClusterInterface * serverClusterInterface = nullptr;
        RegisteredServerClusterInterface * next         = nullptr;

        constexpr RegisteredServerClusterInterface(ServerClusterInterface * cluster, RegisteredServerClusterInterface * n) :
            serverClusterInterface(cluster), next(n)
        {}
    };

    /// tracks clusters registered to a particular endpoint
    struct EndpointClusters
    {
        // The endpointId for this registration. kInvalidEndpointId means
        // not allocated/used
        EndpointId endpointId = kInvalidEndpointId;

        // A single-linked list of clusters registered for the given `endpointId`
        RegisteredServerClusterInterface * firstCluster = nullptr;

        /// A single-linked list of endpoint clusters that is dynamically
        /// allocated.
        EndpointClusters * next;
    };

    /// returns nullptr if not found
    EndpointClusters * FindClusters(EndpointId endpointId);

    /// Get a new usable endpoint cluster
    CHIP_ERROR AllocateNewEndpointClusters(EndpointId endpointId, EndpointClusters *& dest);

    /// Clear and free memory for the given linked list
    static void ClearSingleLinkedList(RegisteredServerClusterInterface * clusters);

    // Dynamic allocated endpoint cluters.
    EndpointClusters * mEndpoints = nullptr;

    // A one-element cache to speed up finding a cluster within an endpoint.
    // The endpointId specifies which endpoint the cache belongs to.
    ClusterId mCachedClusterEndpointId        = kInvalidEndpointId;
    ServerClusterInterface * mCachedInterface = nullptr;
};

} // namespace app
} // namespace chip
