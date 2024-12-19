/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#include "lib/core/DataModelTypes.h"
#include <app/AttributePathParams.h>
#include <app/ConcreteAttributePath.h>
#include <app/data-model-provider/Provider.h>
#include <lib/support/LinkedList.h>

namespace chip {
namespace app {

/// Handles attribute path expansions
/// Usage:
///
/// - Start iterating by creating an iteration state
///
///      mState =  AttributePathExpandIterator2::State::StartIterating(path);
///
/// - Use the iteration state in a for loop:
///
///      ConcreteAttributePath path;
///      for (AttributePathExpandIterator2 iterator(mState); iterator->Next(path);) {
///         // use `path` here`
///      }
///
///   OR:
///
///      ConcreteAttributePath path;
///      AttributePathExpandIterator2 iterator(mState);
///
///      while (iterator.Next(path)) {
///         // use `path` here`
///      }
///
/// USAGE requirements and assumptions:
///
///    - There should be only one single AttributePathExpandIterator2 for a state  at a time.
///
///    - `State` is automatically updated by the AttributePathExpandIterator2, so
///      calling `Next` on the iterator will update the state variable.
///
///
class AttributePathExpandIterator2
{
public:
    class State
    {
    public:
        friend class AttributePathExpandIterator2;

        /// External callers can only ever start iterating on a new path from the beginning
        static State StartIterating(SingleLinkedListNode<AttributePathParams> * path) { return State(path); }

        /// Copies are allowed
        State(const State &)             = default;
        State & operator=(const State &) = default;

        State() : mAttributePath(nullptr) {}

        /// Reset the iterator to the beginning of current cluster if we are in the middle of expanding a wildcard attribute id for
        /// some cluster.
        ///
        /// When attributes are changed in the middle of expanding a wildcard attribute, we need to reset the iterator, to provide
        /// the client with a consistent state of the cluster.
        void IterateFromTheStartOfTheCurrentCluster()
        {
            VerifyOrReturn(mAttributePath != nullptr && mAttributePath->mValue.HasWildcardAttributeId());
            mLastOutputPath.mAttributeId = kInvalidAttributeId;
            mLastOutputPath.mExpanded    = true;
        }

        /// Fetch the last output path by this expand iterator
        ///
        /// Returns false if the current iteration is completed
        bool GetLastOutputPath(ConcreteAttributePath & path)
        {
            path = mLastOutputPath;
            return (mAttributePath != nullptr);
        }

    protected:
        State(SingleLinkedListNode<AttributePathParams> * path) :
            mAttributePath(path), mLastOutputPath(kInvalidEndpointId, kInvalidClusterId, kInvalidAttributeId)
        {
            mLastOutputPath.mExpanded = true;
        }

        SingleLinkedListNode<AttributePathParams> * mAttributePath;
        ConcreteAttributePath mLastOutputPath;
    };

    AttributePathExpandIterator2(DataModel::Provider * dataModel, State & state) : mDataModelProvider(dataModel), mState(state) {}

    // this class may not be copied. A new one should be created when needed and they
    // should not overlap
    AttributePathExpandIterator2(const AttributePathExpandIterator2 &)             = delete;
    AttributePathExpandIterator2 & operator=(const AttributePathExpandIterator2 &) = delete;

    /// Get the next path of the expansion (if one exists).
    ///
    /// On success, true is returned and `path` is filled with the next path in the
    /// expansion.
    /// On iteration completion, false is returned and the content of path IS NOT DEFINED.
    bool Next(ConcreteAttributePath & path);

private:
    DataModel::Provider * mDataModelProvider;
    State & mState;

    /// Move to the next endpoint/cluster/attribute triplet that is valid given
    /// the current mOutputPath and mpAttributePath
    ///
    /// returns true if such a next value was found.
    bool AdvanceOutputPath();

    /// Get the next attribute ID in mOutputPath(endpoint/cluster) if one is available.
    /// Will start from the beginning if current mOutputPath.mAttributeId is kInvalidAttributeId
    ///
    /// Respects path expansion/values in mpAttributePath
    ///
    /// Handles Global attributes (which are returned at the end)
    std::optional<AttributeId> NextAttributeId();

    /// Get the next cluster ID in mOutputPath(endpoint) if one is available.
    /// Will start from the beginning if current mOutputPath.mClusterId is kInvalidClusterId
    ///
    /// Respects path expansion/values in mpAttributePath
    std::optional<ClusterId> NextClusterId();

    /// Get the next endpoint ID in mOutputPath if one is available.
    /// Will start from the beginning if current mOutputPath.mEndpointId is kInvalidEndpointId
    ///
    /// Respects path expansion/values in mpAttributePath
    std::optional<ClusterId> NextEndpointId();

    /// Checks if the given attributeId is valid for the current mOutputPath(endpoint/cluster)
    ///
    /// Meaning that it is known to the data model OR it is a always-there global attribute.
    bool IsValidAttributeId(AttributeId attributeId);
};

/**
 * AttributePathExpandIterator is used to iterate over a linked list of AttributePathParams-s.
 * The AttributePathExpandIterator is copiable, however, the given cluster info must be valid when calling Next().
 *
 * AttributePathExpandIterator will expand attribute paths with wildcards, and only emit existing paths for
 * AttributePathParams with wildcards. For AttributePathParams with a concrete path (i.e. does not contain wildcards),
 * AttributePathExpandIterator will emit them as-is.
 *
 * The typical use of AttributePathExpandIterator may look like:
 * ConcreteAttributePath path;
 * for (AttributePathExpandIterator iterator(AttributePathParams); iterator.Get(path); iterator.Next()) {...}
 *
 * The iterator does not copy the given AttributePathParams. The given AttributePathParams must remain valid when using the
 * iterator. If the set of endpoints, clusters, or attributes that are supported changes, AttributePathExpandIterator must be
 * reinitialized.
 *
 * A initialized iterator will return the first valid path, no need to call Next() before calling Get() for the first time.
 *
 * Note: Next() and Get() are two separate operations by design since a possible call of this iterator might be:
 * - Get()
 * - Chunk full, return
 * - In a new chunk, Get()
 *
 * TODO: The AttributePathParams may support a group id, the iterator should be able to call group data provider to expand the group
 * id.
 */
class AttributePathExpandIterator
{
public:
    AttributePathExpandIterator(DataModel::Provider * provider, SingleLinkedListNode<AttributePathParams> * attributePath);

    /**
     * Proceed the iterator to the next attribute path in the given cluster info.
     *
     * Returns false if AttributePathExpandIteratorDataModeDataModel has exhausted all paths in the given AttributePathParams list.
     */
    bool Next();

    /**
     * Fills the aPath with the path the iterator currently points to.
     * Returns false if the iterator is not pointing to a valid path (i.e. it has exhausted the cluster info).
     */
    bool Get(ConcreteAttributePath & aPath)
    {
        aPath = mOutputPath;
        return (mpAttributePath != nullptr);
    }

    /**
     * Reset the iterator to the beginning of current cluster if we are in the middle of expanding a wildcard attribute id for some
     * cluster.
     *
     * When attributes are changed in the middle of expanding a wildcard attribute, we need to reset the iterator, to provide the
     * client with a consistent state of the cluster.
     */
    void ResetCurrentCluster();

    /** Start iterating over the given `paths` */
    inline void ResetTo(SingleLinkedListNode<AttributePathParams> * paths)
    {
        *this = AttributePathExpandIterator(mDataModelProvider, paths);
    }

private:
    DataModel::Provider * mDataModelProvider;
    SingleLinkedListNode<AttributePathParams> * mpAttributePath;
    ConcreteAttributePath mOutputPath;

    /// Move to the next endpoint/cluster/attribute triplet that is valid given
    /// the current mOutputPath and mpAttributePath
    ///
    /// returns true if such a next value was found.
    bool AdvanceOutputPath();

    /// Get the next attribute ID in mOutputPath(endpoint/cluster) if one is available.
    /// Will start from the beginning if current mOutputPath.mAttributeId is kInvalidAttributeId
    ///
    /// Respects path expansion/values in mpAttributePath
    ///
    /// Handles Global attributes (which are returned at the end)
    std::optional<AttributeId> NextAttributeId();

    /// Get the next cluster ID in mOutputPath(endpoint) if one is available.
    /// Will start from the beginning if current mOutputPath.mClusterId is kInvalidClusterId
    ///
    /// Respects path expansion/values in mpAttributePath
    std::optional<ClusterId> NextClusterId();

    /// Get the next endpoint ID in mOutputPath if one is available.
    /// Will start from the beginning if current mOutputPath.mEndpointId is kInvalidEndpointId
    ///
    /// Respects path expansion/values in mpAttributePath
    std::optional<ClusterId> NextEndpointId();

    /// Checks if the given attributeId is valid for the current mOutputPath(endpoint/cluster)
    ///
    /// Meaning that it is known to the data model OR it is a always-there global attribute.
    bool IsValidAttributeId(AttributeId attributeId);
};

} // namespace app
} // namespace chip
