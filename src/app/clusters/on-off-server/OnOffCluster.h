/*
 *    Copyright (c) 2026 Project CHIP Authors
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

#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Commands.h>
#include <app/server-cluster/DefaultServerCluster.h>

#include "OnOffDelegate.h"

namespace chip::app::Clusters::OnOff {

/// Implements an On/Off server cluster.
///
/// *DOES NOT* support the Lighting feature (to keep this implementation small)
class OnOffCluster : public DefaultServerCluster
{
public:
    /// The delegate must outlive the cluster instance.
    OnOffCluster(EndpointId endpointId, OnOffDelegate & delegate, BitMask<Feature> featureMap = {});
    ~OnOffCluster() override = default;

    /// Sets the OnOff attribute.
    ///
    /// This also handles writing the new value into non-volatile storage and
    /// notifying the delegate.
    virtual CHIP_ERROR SetOnOff(bool on);

    virtual bool GetOnOff() const;

    // ServerClusterInterface methods
    CHIP_ERROR Startup(ServerClusterContext & context) override;

    CHIP_ERROR Attributes(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder) override;

    CHIP_ERROR AcceptedCommands(const ConcreteClusterPath & path,
                                ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> & builder) override;

    DataModel::ActionReturnStatus ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                AttributeValueEncoder & encoder) override;

    std::optional<DataModel::ActionReturnStatus> InvokeCommand(const DataModel::InvokeRequest & request,
                                                               chip::TLV::TLVReader & input_arguments,
                                                               CommandHandler * handler) override;

protected:
    /// Allows derived classes to specify the subset of OnOff features they implement.
    /// Validates that requested 'featureMap' does not exceed implementation capabilities.
    ///
    /// This will VerifyOrDie that featureMap is a subset of supportedFeatures.
    OnOffCluster(EndpointId endpointId, OnOffDelegate & delegate, BitMask<Feature> featureMap, BitMask<Feature> supportedFeatures);

    OnOffDelegate & mDelegate;
    BitMask<Feature> mFeatureMap;

    // Attribute local storage
    bool mOnOff = false;
};

} // namespace chip::app::Clusters::OnOff
