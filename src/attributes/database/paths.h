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

#include <attributes/database/type_safe_id.h>

namespace chip {
namespace Attributes {

namespace Cluster {
/// Represents a path to a cluster, by IDs
class Path
{
public:
    Path(Endpoint::Id endpoint, Cluster::Id cluster) : mEndpoint(endpoint), mCluster(cluster) {}

    static Path Invalid() { return Path(Endpoint::Id::Invalid(), Cluster::Id::Invalid()); }

    bool operator==(const Path & other) const
    {
        return (mEndpoint == other.mEndpoint) //
            && (mCluster == other.mCluster);  //
    }
    bool operator!=(const Path & other) const { return !(*this == other); }

    bool IsValid() const { return mEndpoint.IsValid() && mCluster.IsValid(); }

    Endpoint::Id GetEndpoint() const { return mEndpoint; }
    Cluster::Id GetCluster() const { return mCluster; }

private:
    Endpoint::Id mEndpoint;
    Cluster::Id mCluster;
};

/// Represents a path to a cluster by Indexes into some array-backed storage
class IndexPath
{
public:
    IndexPath(Endpoint::Index endpoint, Cluster::Index cluster) : mEndpoint(endpoint), mCluster(cluster) {}
    static IndexPath Invalid() { return IndexPath(Endpoint::Index::Invalid(), Cluster::Index::Invalid()); }
    bool IsValid() const { return mEndpoint.IsValid() && mCluster.IsValid(); }

    bool operator==(const IndexPath & other) const
    {
        return (mEndpoint == other.mEndpoint) //
            && (mCluster == other.mCluster);  //
    }
    bool operator!=(const IndexPath & other) const { return !(*this == other); }

    Endpoint::Index GetEndpoint() const { return mEndpoint; }
    Cluster::Index GetCluster() const { return mCluster; }

private:
    Endpoint::Index mEndpoint;
    Cluster::Index mCluster;
};

} // namespace Cluster

namespace Attribute {

/// Represents a path to an attribute, by IDs
class Path
{
public:
    Path(Endpoint::Id endpoint, Cluster::Id cluster, Attribute::Id attribute) :
        mEndpoint(endpoint), mCluster(cluster), mAttribute(attribute)
    {}

    Path(const Cluster::Path clusterPath, Attribute::Id attribute) :
        mEndpoint(clusterPath.GetEndpoint()), mCluster(clusterPath.GetCluster()), mAttribute(attribute)
    {}

    static Path Invalid() { return Path(Endpoint::Id::Invalid(), Cluster::Id::Invalid(), Attribute::Id::Invalid()); }

    bool IsValid() const { return mEndpoint.IsValid() && mCluster.IsValid() && mAttribute.IsValid(); }

    bool operator==(const Path & other) const
    {
        return (mEndpoint == other.mEndpoint)    //
            && (mCluster == other.mCluster)      //
            && (mAttribute == other.mAttribute); //
    }

    bool operator!=(const Path & other) const { return !(*this == other); }

    Endpoint::Id GetEndpoint() const { return mEndpoint; }
    Cluster::Id GetCluster() const { return mCluster; }
    Attribute::Id GetAttribute() const { return mAttribute; }

    Cluster::Path GetClusterPath() const {return Cluster::Path(mEndpoint, mCluster);}

private:
    Endpoint::Id mEndpoint;
    Cluster::Id mCluster;
    Attribute::Id mAttribute;
};

/// Represents a path to an attribute, by Indexes
class IndexPath
{
public:
    IndexPath(Endpoint::Index endpoint, Cluster::Index cluster, Attribute::Index attribute) :
        mEndpoint(endpoint), mCluster(cluster), mAttribute(attribute)
    {}

    IndexPath(const Cluster::IndexPath clusterPath, Attribute::Index attribute) :
        mEndpoint(clusterPath.GetEndpoint()), mCluster(clusterPath.GetCluster()), mAttribute(attribute)
    {}

    static IndexPath Invalid()
    {
        return IndexPath(Endpoint::Index::Invalid(), Cluster::Index::Invalid(), Attribute::Index::Invalid());
    }

    bool IsValid() const { return mEndpoint.IsValid() && mCluster.IsValid() && mAttribute.IsValid(); }

    bool operator==(const IndexPath & other) const
    {
        return (mEndpoint == other.mEndpoint)    //
            && (mCluster == other.mCluster)      //
            && (mAttribute == other.mAttribute); //
    }

    bool operator!=(const IndexPath & other) const { return !(*this == other); }

    Endpoint::Index GetEndpoint() const { return mEndpoint; }
    Cluster::Index GetCluster() const { return mCluster; }
    Attribute::Index GetAttribute() const { return mAttribute; }

    Cluster::IndexPath GetClusterPath() const {return Cluster::IndexPath(mEndpoint, mCluster);}
private:
    Endpoint::Index mEndpoint;
    Cluster::Index mCluster;
    Attribute::Index mAttribute;
};

} // namespace Attribute

} // namespace Attributes
} // namespace chip
