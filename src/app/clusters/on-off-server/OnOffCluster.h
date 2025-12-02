/**
 *    Copyright (c) 2025 Project CHIP Authors
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

#include <app/server-cluster/DefaultServerCluster.h>
#include <clusters/OnOff/Enums.h>
#include <clusters/OnOff/Ids.h>

namespace chip::app::Clusters {

/// Implements a On/Off cluster WITHOUT Lighting support.
///
/// Automatically handles NVM persistence of the OnOff attribute and
/// uses a Delegate to report when on-off values change
class OnOffCluster : public DefaultServerCluster
{
public:
    /// Represents a delegate that is called when on-off operations occur
    class Delegate
    {
    public:
        enum class ChangeReason
        {
            kUnknown = 0,
            kStartup,
            kOffCommand,
            kOnCommand,
            kToggleCommand,
        };

        virtual ~Delegate() = default;

        /// Will be called whenever the underlying on/off value changes.
        ///
        /// - Is called *after* the current value is changed (i.e. `IsOn` returns the new value).
        /// - will be called on startup *after* the previous value is loaded from NVRAM.
        /// - Can safely call "SetOn" (it will not trigger a recursive delegate call)
        virtual void OnOffChanged(OnOffCluster & cluster, ChangeReason reason) = 0;
    };

    // a subset of supported features. In particular Lighting is not supported
    enum class Feature : uint32_t
    {
        kDeadFrontBehavior = to_underlying(OnOff::Feature::kDeadFrontBehavior),
        kOffOnly           = to_underlying(OnOff::Feature::kOffOnly),
    };

    using FeatureMap = BitFlags<Feature>;

    OnOffCluster(EndpointId endpointId, Delegate & delegate, FeatureMap features = {}) :
        DefaultServerCluster({ endpointId, OnOff::Id }), mFeatures(features), mDelegate(delegate)
    {}

    Delegate &GetDelegate() const { return mDelegate; }

    bool IsOn() const { return mOn; }

    /// Set the current `OnOff` value. This *does not* trigger a delegate call
    /// and instead directly updates the on/off value.
    void SetOn(bool value);

    /// ServerClusterInterface Implementation
    CHIP_ERROR Startup(ServerClusterContext & context) override;
    CHIP_ERROR Attributes(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder) override;
    DataModel::ActionReturnStatus ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                AttributeValueEncoder & encoder) override;
    std::optional<DataModel::ActionReturnStatus> InvokeCommand(const DataModel::InvokeRequest & request,
                                                               chip::TLV::TLVReader & input_arguments,
                                                               CommandHandler * handler) override;
    CHIP_ERROR AcceptedCommands(const ConcreteClusterPath & path,
                                ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> & builder) override;

private:
    const FeatureMap mFeatures;
    Delegate & mDelegate;
    bool mOn = false;

    void InternalSetOn(bool value, Delegate::ChangeReason reason);
};

} // namespace chip::app::Clusters
