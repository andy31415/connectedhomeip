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

#include <app/server-cluster/DefaultServerCluster.h>
#include <clusters/Groups/Commands.h>
#include <clusters/Groups/Ids.h>
#include <credentials/GroupDataProvider.h>

namespace chip::app::Clusters {

class GroupsCluster : public DefaultServerCluster
{
public:
    GroupsCluster(EndpointId endpointId, Credentials::GroupDataProvider & groupDataProvider) :
        DefaultServerCluster({ endpointId, Groups::Id }), mGroupDataProvider(groupDataProvider)
    {}

    // ServerClusterInterface
    CHIP_ERROR Attributes(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder) override;
    CHIP_ERROR AcceptedCommands(const ConcreteClusterPath & path,
                                ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> & builder) override;
    CHIP_ERROR GeneratedCommands(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<CommandId> & builder) override;

    DataModel::ActionReturnStatus ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                AttributeValueEncoder & encoder) override;
    std::optional<DataModel::ActionReturnStatus> InvokeCommand(const DataModel::InvokeRequest & request,
                                                               chip::TLV::TLVReader & input_arguments,
                                                               CommandHandler * handler) override;

private:
    Credentials::GroupDataProvider & mGroupDataProvider;

    std::optional<DataModel::ActionReturnStatus> HandleGroupAdd(const Groups::Commands::AddGroup::DecodableType & input,
                                                                CommandHandler * handler);
};

} // namespace chip::app::Clusters
