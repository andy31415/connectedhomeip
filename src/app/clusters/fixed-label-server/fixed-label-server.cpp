/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

/****************************************************************************
 * @file
 * @brief Implementation for the Fixed Label Server Cluster
 ***************************************************************************/

#include <protocols/interaction_model/StatusCode.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/AttributeAccessInterface.h>
#include <app/AttributeAccessInterfaceRegistry.h>
#include <app/server-cluster/DefaultServerCluster.h>
#include <app/server-cluster/ServerClusterInterface.h>
#include <app/server-cluster/ServerClusterInterfaceRegistry.h>
#include <app/util/attribute-storage.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/DeviceInfoProvider.h>
#include <platform/PlatformManager.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::FixedLabel;
using namespace chip::app::Clusters::FixedLabel::Attributes;

namespace {

class FixedLabelAttrAccess : public DefaultServerCluster
{
public:
    // Register for the Fixed Label cluster on all endpoints.
    FixedLabelAttrAccess() = default;

    ClusterId GetClusterId() const override { return FixedLabel::Id; }

    CHIP_ERROR Attributes(const ConcreteClusterPath & path, DataModel::ListBuilder<AttributeEntry> & builder) override;
    ActionReturnStatus ReadAttribute(const ReadAttributeRequest & request, AttributeValueEncoder & encoder) override;

private:
    CHIP_ERROR ReadLabelList(EndpointId endpoint, AttributeValueEncoder & aEncoder);
};

CHIP_ERROR FixedLabelAttrAccess::Attributes(const ConcreteClusterPath & path, DataModel::ListBuilder<AttributeEntry> & builder)
{
    static constexpr AttributeEntry kAttributes[] = {
        { LabelList::Id, AttributeQualityFlags::kListAttribute, Access::Privilege::kView },
    };

    ReturnErrorOnFailure(builder.ReferenceExisting(Span<const AttributeEntry>(kAttributes)));
    return builder.AppendElements(GetGlobalAttributes());
}

CHIP_ERROR FixedLabelAttrAccess::ReadLabelList(EndpointId endpoint, AttributeValueEncoder & aEncoder)
{
    DeviceLayer::DeviceInfoProvider * provider = DeviceLayer::GetDeviceInfoProvider();
    if (provider == nullptr)
    {
        return aEncoder.EncodeEmptyList();
    }

    DeviceLayer::DeviceInfoProvider::FixedLabelIterator * it = provider->IterateFixedLabel(endpoint);
    if (it == nullptr)
    {
        return aEncoder.EncodeEmptyList();
    }

    CHIP_ERROR err = aEncoder.EncodeList([&it](const auto & encoder) -> CHIP_ERROR {
        FixedLabel::Structs::LabelStruct::Type fixedlabel;

        while (it->Next(fixedlabel))
        {
            ReturnErrorOnFailure(encoder.Encode(fixedlabel));
        }

        return CHIP_NO_ERROR;
    });

    it->Release();

    return err;
}

FixedLabelAttrAccess gAttrAccess;

ActionReturnStatus FixedLabelAttrAccess::ReadAttribute(const ReadAttributeRequest & request, AttributeValueEncoder & encoder)
{

    switch (request.path.mAttributeId)
    {
    case LabelList::Id:
        return ReadLabelList(request.path.mEndpointId, encoder);
    default:
        return Protocols::InteractionModel::Status::UnsupportedAttribute;
    }
}
} // anonymous namespace

void MatterFixedLabelPluginServerInitCallback()
{
    static_assert(MATTER_DM_FIXED_LABEL_CLUSTER_SERVER_ENDPOINT_COUNT == 1, "Code expects general diagnostics on EP0 only");
    (void) ServerClusterInterfaceRegistry::Instance().Register(kRootEndpointId, &gAttrAccess);
}
