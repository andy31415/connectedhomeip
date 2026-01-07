/*
 *
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

#include "OnOffCluster.h"

#include <app/SafeAttributePersistenceProvider.h>
#include <app/server-cluster/AttributeListBuilder.h>
#include <clusters/OnOff/Metadata.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::OnOff;
using chip::Protocols::InteractionModel::Status;

namespace chip {
namespace app {
namespace Clusters {
namespace OnOff {

OnOffCluster::OnOffCluster(EndpointId endpointId, OnOffDelegate & delegate) :
    DefaultServerCluster({ endpointId, Clusters::OnOff::Id }), mDelegate(delegate)
{
}

CHIP_ERROR OnOffCluster::Startup(ServerClusterContext & context)
{
    ReturnErrorOnFailure(DefaultServerCluster::Startup(context));
    LoadPersistentAttributes();
    return CHIP_NO_ERROR;
}

CHIP_ERROR OnOffCluster::AcceptedCommands(const ConcreteClusterPath & path,
                                          ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> & builder)
{
    static constexpr DataModel::AcceptedCommandEntry kAcceptedCommands[] = {
        Commands::Off::kMetadataEntry,
        Commands::On::kMetadataEntry,
        Commands::Toggle::kMetadataEntry,
    };
    return builder.ReferenceExisting(kAcceptedCommands);
}

CHIP_ERROR OnOffCluster::Attributes(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder)
{
    AttributeListBuilder listBuilder(builder);
    return listBuilder.Append(Span(Attributes::kMandatoryMetadata), {});
}

void OnOffCluster::LoadPersistentAttributes()
{
    bool storedOnOff = false;
    CHIP_ERROR err = GetSafeAttributePersistenceProvider()->ReadScalarValue(
        ConcreteAttributePath(mPath.mEndpointId, Clusters::OnOff::Id, Attributes::OnOff::Id), storedOnOff);
    if (err == CHIP_NO_ERROR)
    {
        mOnOff = storedOnOff;
    }
}

DataModel::ActionReturnStatus OnOffCluster::ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                          AttributeValueEncoder & encoder)
{
    switch (request.path.mAttributeId)
    {
    case Attributes::ClusterRevision::Id:
        return encoder.Encode(kRevision);
    case Attributes::FeatureMap::Id:
        return encoder.Encode(static_cast<uint32_t>(0)); // No features yet
    case Attributes::OnOff::Id:
        return encoder.Encode(mOnOff);
    default:
        return Protocols::InteractionModel::Status::UnsupportedAttribute;
    }
}

DataModel::ActionReturnStatus OnOffCluster::WriteAttribute(const DataModel::WriteAttributeRequest & request,
                                                           AttributeValueDecoder & decoder)
{
    // OnOff is ReadOnly.
    return Protocols::InteractionModel::Status::UnsupportedAttribute;
}

Status OnOffCluster::SetOnOff(bool on)
{
    if (mOnOff != on)
    {
        mOnOff = on;
        NotifyAttributeChanged(Attributes::OnOff::Id);
        
        // Persist
        TEMPORARY_RETURN_IGNORED GetSafeAttributePersistenceProvider()->WriteScalarValue(
            { mPath.mEndpointId, Clusters::OnOff::Id, Attributes::OnOff::Id }, mOnOff);
        
        mDelegate.OnOnOffChanged(mOnOff);
    }
    return Status::Success;
}

std::optional<DataModel::ActionReturnStatus> OnOffCluster::InvokeCommand(const DataModel::InvokeRequest & request,
                                                                         chip::TLV::TLVReader & input_arguments,
                                                                         CommandHandler * handler)
{
    switch (request.path.mCommandId)
    {
    case Commands::Off::Id:
        return DataModel::ActionReturnStatus(SetOnOff(false));
    case Commands::On::Id:
        return DataModel::ActionReturnStatus(SetOnOff(true));
    case Commands::Toggle::Id:
        return DataModel::ActionReturnStatus(SetOnOff(!mOnOff));
    default:
        return Status::UnsupportedCommand;
    }
}

bool OnOffCluster::GetOnOff() const
{
    return mOnOff;
}

} // namespace OnOff
} // namespace Clusters
} // namespace app
} // namespace chip
