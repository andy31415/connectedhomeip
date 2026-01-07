/*
 *
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

#include <app/server-cluster/DefaultServerCluster.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Commands.h>
#include <app-common/zap-generated/ids/Clusters.h>

namespace chip {
namespace app {
namespace Clusters {
namespace OnOff {

class OnOffDelegate;

class OnOffCluster : public DefaultServerCluster
{
public:
    /**
     * Creates an OnOff Cluster instance.
     * @param endpointId The endpoint on which this cluster exists.
     * @param delegate A reference to the delegate to be used by this server.
     * Note: the caller must ensure that the delegate lives throughout the instance's lifetime.
     */
    OnOffCluster(EndpointId endpointId, OnOffDelegate & delegate);
    ~OnOffCluster() override;

    // Attribute Setters
    /**
     * Sets the OnOff attribute. Note, this also handles writing the new value into non-volatile storage.
     * @param on The value to which the OnOff is to be set.
     * @return Returns Success if successful, otherwise an error status.
     */
    Protocols::InteractionModel::Status SetOnOff(bool on);

    // Attribute Getters
    /**
     * @return The Current OnOff state.
     */
    bool GetOnOff() const;

    /**
     * @brief ServerClusterInterface methods.
     */
    CHIP_ERROR Startup(ServerClusterContext & context) override;

    CHIP_ERROR Attributes(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder) override;

    CHIP_ERROR AcceptedCommands(const ConcreteClusterPath & path,
                                ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> & builder) override;

    DataModel::ActionReturnStatus ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                AttributeValueEncoder & encoder) override;
    DataModel::ActionReturnStatus WriteAttribute(const DataModel::WriteAttributeRequest & request,
                                                 AttributeValueDecoder & decoder) override;

    std::optional<DataModel::ActionReturnStatus> InvokeCommand(const DataModel::InvokeRequest & request,
                                                               chip::TLV::TLVReader & input_arguments,
                                                               CommandHandler * handler) override;

private:
    OnOffDelegate & mDelegate;

    // Attribute local storage
    bool mOnOff = false;

    // Helpers
    // Loads all the persistent attributes from the KVS.
    void LoadPersistentAttributes();
};

/** @brief
 *  Defines methods for implementing application-specific logic for the OnOff Cluster.
 */
class OnOffDelegate
{
public:
    OnOffDelegate() = default;

    virtual ~OnOffDelegate() = default;

    /**
     * @brief Called when the OnOff attribute has changed.
     * The delegate should update the hardware state to match the new value.
     *
     * @param on The new OnOff state.
     */
    virtual void OnOnOffChanged(bool on) = 0;

protected:
    friend class OnOffCluster;

    OnOffCluster * mOnOffCluster = nullptr;

    void SetOnOffCluster(OnOffCluster * onOffCluster) { mOnOffCluster = onOffCluster; }
    OnOffCluster * GetOnOffCluster() const { return mOnOffCluster; }
};

} // namespace OnOff
} // namespace Clusters
} // namespace app
} // namespace chip
