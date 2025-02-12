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
#include <app/server-cluster/ServerClusterInterfaceRegistry.h>

#include <app/ConcreteClusterPath.h>
#include <app/server-cluster/ServerClusterInterface.h>
#include <lib/core/DataModelTypes.h>
#include <lib/support/CodeUtils.h>

namespace chip {
namespace app {

ServerClusterInterfaceRegistry & ServerClusterInterfaceRegistry::Instance()
{
    static ServerClusterInterfaceRegistry sRegistry;
    return sRegistry;
}

CHIP_ERROR ServerClusterInterfaceRegistry::AllocateNewEndpointClusters(EndpointId endpointId, EndpointClusters *& dest)
{
    for (auto & ep : mPreallocateEndpoints)
    {
        if (ep.endpointId == kInvalidEndpointId)
        {
            ep.endpointId   = endpointId;
            ep.firstCluster = nullptr;
            dest            = &ep;
            return CHIP_NO_ERROR;
        }
    }

    // need to allocate dynamically
    auto result = Platform::New<DynamicEndpointClusters>();
    VerifyOrReturnError(result != nullptr, CHIP_ERROR_NO_MEMORY);

    result->endpointId   = endpointId;
    result->firstCluster = nullptr;
    result->next         = mDynamicEndpoints;
    mDynamicEndpoints    = result;
    dest                 = result;

    return CHIP_NO_ERROR;
}

CHIP_ERROR ServerClusterInterfaceRegistry::Register(EndpointId endpointId, ServerClusterInterface * cluster)
{
    VerifyOrReturnError(cluster != nullptr, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(!cluster->IsInList(), CHIP_ERROR_IN_USE);
    VerifyOrReturnError(endpointId != kInvalidEndpointId, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(cluster->GetClusterId() != kInvalidClusterId, CHIP_ERROR_INVALID_ARGUMENT);

    // duplicate registrations are disallowed
    VerifyOrReturnError(Get(ConcreteClusterPath(endpointId, cluster->GetClusterId())) == nullptr, CHIP_ERROR_DUPLICATE_KEY_ID);

    EndpointClusters * endpointClusters = FindClusters(endpointId);
    if (endpointClusters == nullptr)
    {
        ReturnErrorOnFailure(AllocateNewEndpointClusters(endpointId, endpointClusters));
    }

    cluster->SetNextListItem(endpointClusters->firstCluster);
    endpointClusters->firstCluster = cluster;

    return CHIP_NO_ERROR;
}

ServerClusterInterface * ServerClusterInterfaceRegistry::Unregister(const ConcreteClusterPath & path)
{
    EndpointClusters * endpointClusters = FindClusters(path.mEndpointId);
    VerifyOrReturnValue(endpointClusters != nullptr, nullptr);
    VerifyOrReturnValue(endpointClusters->firstCluster != nullptr, nullptr);

    if (endpointClusters->firstCluster->GetClusterId() == path.mClusterId)
    {
        ServerClusterInterface * previous = endpointClusters->firstCluster;
        endpointClusters->firstCluster    = endpointClusters->firstCluster->GetNextListItem();

        if (endpointClusters->firstCluster == nullptr)
        {
            // Free up the endpoint as it has nothing on it.
            UnregisterAllFromEndpoint(endpointClusters->endpointId);
        }

        // ensure we can catch if elements are parts of list or not
        previous->SetNotInList();
        return previous;
    }

    // not found, go through the underlying list and remove any found element
    ServerClusterInterface * prev    = endpointClusters->firstCluster;
    ServerClusterInterface * current = endpointClusters->firstCluster->GetNextListItem();

    while (current != nullptr)
    {
        if (current->GetClusterId() == path.mClusterId)
        {
            // takes the item out of the current list and return it.
            prev->SetNextListItem(current->GetNextListItem());

            // ensure we can catch if elements are parts of list or not
            current->SetNotInList();
            return current;
        }

        prev    = current;
        current = current->GetNextListItem();
    }

    // Not found.
    return nullptr;
}

void ServerClusterInterfaceRegistry::DestroySingleLinkedList(ServerClusterInterface * clusters)
{
    while (clusters != nullptr)
    {
        ServerClusterInterface * next = clusters->GetNextListItem();
        clusters->SetNotInList();
        clusters = next;
    }
}

void ServerClusterInterfaceRegistry::UnregisterAllFromEndpoint(EndpointId endpointId)
{
    if ((mEndpointClustersCache != nullptr) && (mEndpointClustersCache->endpointId == endpointId))
    {
        mEndpointClustersCache = nullptr;
    }

    // if it is static, just clear it
    for (auto & ep : mPreallocateEndpoints)
    {
        if (ep.endpointId == endpointId)
        {
            ep.endpointId = kInvalidEndpointId;
            DestroySingleLinkedList(ep.firstCluster);
            ep.firstCluster = nullptr;
            return;
        }
    }

    // not found, search dynamic. Special handling for first;
    VerifyOrReturn(mDynamicEndpoints != nullptr);
    if (mDynamicEndpoints->endpointId == endpointId)
    {
        DynamicEndpointClusters * value = mDynamicEndpoints;
        mDynamicEndpoints               = mDynamicEndpoints->next;
        DestroySingleLinkedList(value->firstCluster);
        Platform::Delete(value);
        return;
    }
    DynamicEndpointClusters * prev    = mDynamicEndpoints;
    DynamicEndpointClusters * current = prev->next;

    while (current != nullptr)
    {
        if (current->endpointId == endpointId)
        {
            prev->next = current->next;
            DestroySingleLinkedList(current->firstCluster);
            Platform::Delete(current);
            return;
        }

        prev    = current;
        current = current->next;
    }
}

ServerClusterInterface * ServerClusterInterfaceRegistry::Get(const ConcreteClusterPath & path)
{
    EndpointClusters * endpointClusters = FindClusters(path.mEndpointId);
    VerifyOrReturnValue(endpointClusters != nullptr, nullptr);
    VerifyOrReturnValue(endpointClusters->firstCluster != nullptr, nullptr);

    // To speed up lookups, this search will ALWAYS place any found element
    // at the BEGINNING of the list.
    if (endpointClusters->firstCluster->GetClusterId() == path.mClusterId)
    {
        return endpointClusters->firstCluster;
    }

    // The cluster searched for is not the first. Do a linear search
    // and update the "first" to be the found element (if any)
    ServerClusterInterface * prev    = endpointClusters->firstCluster;
    ServerClusterInterface * current = endpointClusters->firstCluster->GetNextListItem();

    while (current != nullptr)
    {
        if (current->GetClusterId() == path.mClusterId)
        {
            // adjust the list to move the found item at the start
            // firstCluster -> A -> ...... -> prev -> current -> X ....
            prev->SetNextListItem(current->GetNextListItem());
            //                                 ┌─────────────────┐
            //                                 │                 ↓
            // firstCluster -> A -> ...... -> prev    current -> X ....
            current->SetNextListItem(endpointClusters->firstCluster);
            //                                 ┌─────────────────┐
            //                                 │                 ↓
            // firstCluster -> A -> ...... -> prev    current    X ....
            //                 ↑                         │
            //                 └─────────────────────────┘
            endpointClusters->firstCluster = current;
            // firstCluster -> current -> A -> ...... -> prev -> B

            return current;
        }

        prev    = current;
        current = current->GetNextListItem();
    }

    // not found
    return nullptr;
}

ServerClusterInterfaceRegistry::EndpointClusters * ServerClusterInterfaceRegistry::FindClusters(EndpointId endpointId)
{
    // invalid cluster id is NOT acceptable (since static allocation uses the
    // invalid cluster id as a marker of "not used")
    VerifyOrReturnValue(endpointId != kInvalidEndpointId, nullptr);

    if ((mEndpointClustersCache != nullptr) && (mEndpointClustersCache->endpointId == endpointId))
    {
        return mEndpointClustersCache;
    }

    // search statically first
    for (auto & ep : mPreallocateEndpoints)
    {
        if (ep.endpointId == endpointId)
        {
            mEndpointClustersCache = &ep;
            return mEndpointClustersCache;
        }
    }

    // not found, search dynamic
    for (DynamicEndpointClusters * p = mDynamicEndpoints; p != nullptr; p = p->next)
    {
        if (p->endpointId == endpointId)
        {
            mEndpointClustersCache = p;
            return mEndpointClustersCache;
        }
    }

    return nullptr;
}

} // namespace app
} // namespace chip
