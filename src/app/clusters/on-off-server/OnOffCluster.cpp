/*
 *    Copyright (c) 2026 Project CHIP Authors
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

#include <app/persistence/AttributePersistence.h>
#include <app/server-cluster/AttributeListBuilder.h>
#include <clusters/OnOff/Enums.h>
#include <clusters/OnOff/Metadata.h>
#include <lib/core/CHIPError.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

using chip::Protocols::InteractionModel::Status;

namespace chip::app::Clusters::OnOff {

OnOffCluster::OnOffCluster(EndpointId endpointId, OnOffDelegate & delegate, BitMask<Feature> featureMap) :
    OnOffCluster(endpointId, delegate, featureMap, { Feature::kDeadFrontBehavior, Feature::kOffOnly })
{}

OnOffCluster::OnOffCluster(EndpointId endpointId, OnOffDelegate & delegate, BitMask<Feature> featureMap,
                           BitMask<Feature> supportedFeatures) :
    DefaultServerCluster({ endpointId, Clusters::OnOff::Id }),
    mFeatureMap(featureMap)
{
    VerifyOrDie(supportedFeatures.HasAll(featureMap));

    // Feature validity check: offonly does not support any of the other features.
    VerifyOrDie(!featureMap.Has(Feature::kOffOnly) || featureMap.HasOnly(Feature::kOffOnly));
    mDelegates.PushBack(&delegate);
}

OnOffCluster::~OnOffCluster()
{
    mDelegates.Clear();
}

CHIP_ERROR OnOffCluster::Startup(ServerClusterContext & context)
{
    ReturnErrorOnFailure(DefaultServerCluster::Startup(context));

    AttributePersistence attributePersistence(context.attributeStorage);
    attributePersistence.LoadNativeEndianValue(ConcreteAttributePath(mPath.mEndpointId, Clusters::OnOff::Id, Attributes::OnOff::Id),
                                               mOnOff, false);

    for (auto & delegate : mDelegates)
    {
        delegate.OnOffStartup(mOnOff);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR OnOffCluster::AcceptedCommands(const ConcreteClusterPath & path,
                                          ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> & builder)
{
    if (mFeatureMap.Has(Feature::kOffOnly))
    {
        static constexpr DataModel::AcceptedCommandEntry kOffOnlyCommands[] = {
            Commands::Off::kMetadataEntry,
        };
        return builder.ReferenceExisting(kOffOnlyCommands);
    }

    static constexpr DataModel::AcceptedCommandEntry kAllCommands[] = {
        Commands::Off::kMetadataEntry,
        Commands::On::kMetadataEntry,
        Commands::Toggle::kMetadataEntry,
    };
    return builder.ReferenceExisting(kAllCommands);
}

CHIP_ERROR OnOffCluster::Attributes(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder)
{
    AttributeListBuilder listBuilder(builder);
    return listBuilder.Append(Span(Attributes::kMandatoryMetadata), {});
}

DataModel::ActionReturnStatus OnOffCluster::ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                          AttributeValueEncoder & encoder)
{
    switch (request.path.mAttributeId)
    {
    case Attributes::ClusterRevision::Id:
        return encoder.Encode(kRevision);
    case Attributes::FeatureMap::Id:
        return encoder.Encode(mFeatureMap.Raw());
    case Attributes::OnOff::Id:
        return encoder.Encode(mOnOff);
    default:
        return Protocols::InteractionModel::Status::UnsupportedAttribute;
    }
}

CHIP_ERROR OnOffCluster::SetOnOff(bool on)
{
    VerifyOrReturnError(mOnOff != on, CHIP_NO_ERROR);

    mOnOff = on;
    NotifyAttributeChanged(Attributes::OnOff::Id);

    // Persist
    LogErrorOnFailure(
        mContext->attributeStorage.WriteValue(ConcreteAttributePath(mPath.mEndpointId, Clusters::OnOff::Id, Attributes::OnOff::Id),
                                              ByteSpan(reinterpret_cast<const uint8_t *>(&mOnOff), sizeof(mOnOff))));

    for (auto & delegate : mDelegates)
    {
        delegate.OnOnOffChanged(mOnOff);
    }

    return CHIP_NO_ERROR;
}

std::optional<DataModel::ActionReturnStatus> OnOffCluster::InvokeCommand(const DataModel::InvokeRequest & request,
                                                                         chip::TLV::TLVReader & input_arguments,
                                                                         CommandHandler * handler)
{
    // Note: OffOnly feature validation is handled by the Interaction Model by checking the AcceptedCommands list.
    switch (request.path.mCommandId)
    {
    case Commands::Off::Id:
        return SetOnOff(false);
    case Commands::On::Id:
        return SetOnOff(true);
    case Commands::Toggle::Id:
        return SetOnOff(!mOnOff);
    default:
        return Status::UnsupportedCommand;
    }
}

} // namespace chip::app::Clusters::OnOff
