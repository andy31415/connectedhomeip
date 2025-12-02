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
#include <app/clusters/on-off-server/OnOffCluster.h>
#include <app/persistence/AttributePersistence.h>
#include <clusters/OnOff/AttributeIds.h>
#include <clusters/OnOff/Metadata.h>

using namespace chip::app::Clusters::OnOff;

namespace chip::app::Clusters {

CHIP_ERROR OnOffCluster::Attributes(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder)
{
    return builder.ReferenceExisting(OnOff::Attributes::kMandatoryMetadata);
}

DataModel::ActionReturnStatus OnOffCluster::ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                          AttributeValueEncoder & encoder)
{
    switch (request.path.mAttributeId)
    {
    case Attributes::OnOff::Id:
        return encoder.Encode(mOn);
    case Attributes::ClusterRevision::Id:
        return encoder.Encode(OnOff::kRevision);
    case Attributes::FeatureMap::Id:
        // mFeatures is a full subset of the standard cluster features
        return encoder.Encode(mFeatures);
    default:
        return Protocols::InteractionModel::Status::UnreportableAttribute;
    }
}

std::optional<DataModel::ActionReturnStatus> OnOffCluster::InvokeCommand(const DataModel::InvokeRequest & request,
                                                                         chip::TLV::TLVReader & input_arguments,
                                                                         CommandHandler * handler)
{

    switch (request.path.mCommandId)
    {
    case Commands::On::Id:
        InternalSetOn(true, Delegate::ChangeReason::kOnCommand);
        return CHIP_NO_ERROR;
    case Commands::Off::Id:
        InternalSetOn(false, Delegate::ChangeReason::kOffCommand);
        return CHIP_NO_ERROR;
    case Commands::Toggle::Id:
        InternalSetOn(!IsOn(), Delegate::ChangeReason::kToggleCommand);
        return CHIP_NO_ERROR;
    default:
        return Protocols::InteractionModel::Status::UnsupportedCommand;
    }
}

CHIP_ERROR OnOffCluster::AcceptedCommands(const ConcreteClusterPath & path,
                                          ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> & builder)
{
    if (!mFeatures.Has(Feature::kDeadFrontBehavior))
    {
        ReturnErrorOnFailure(builder.AppendElements({
            Commands::On::kMetadataEntry,
            Commands::Toggle::kMetadataEntry,
        }));
    }

    static constexpr DataModel::AcceptedCommandEntry kMandatoryCommands[] = {
        Commands::Off::kMetadataEntry,
    };

    return builder.ReferenceExisting(kMandatoryCommands);
}

void OnOffCluster::SetOn(bool value)
{
    VerifyOrReturn(value != mOn);
    if (mContext != nullptr)
    {
        LogErrorOnFailure(mContext->attributeStorage.WriteValue({ mPath.mEndpointId, OnOff::Id, Attributes::OnOff::Id },
                                                                { reinterpret_cast<uint8_t *>(&mOn), sizeof(mOn) }));
    }
    mOn = value;
    NotifyAttributeChanged(Attributes::OnOff::Id);
}

void OnOffCluster::InternalSetOn(bool value, Delegate::ChangeReason reason)
{
    VerifyOrReturn(value != mOn);
    SetOn(value);
    mDelegate.OnOffChanged(*this, reason);
}

CHIP_ERROR OnOffCluster::Startup(ServerClusterContext & context)
{
    ReturnErrorOnFailure(DefaultServerCluster::Startup(context));

    AttributePersistence persistence(context.attributeStorage);

    RETURN_SAFELY_IGNORED persistence.LoadNativeEndianValue({ mPath.mEndpointId, OnOff::Id, Attributes::OnOff::Id }, mOn, false);

    mDelegate.OnOffChanged(*this, Delegate::ChangeReason::kStartup);

    return CHIP_NO_ERROR;
}

} // namespace chip::app::Clusters
