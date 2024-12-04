/*
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
#include "app/code-data-model-provider/Metadata.h"
#include "app/data-model-provider/MetadataTypes.h"
#include "lib/core/DataModelTypes.h"
#include "pw_unit_test/framework.h"
#include <optional>
#include <pw_unit_test/framework.h>

#include <access/Privilege.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Commands.h>
#include <app/code-data-model-provider/MetadataTree.h>

using namespace chip;
using namespace chip::app;
using namespace chip::Access;
using namespace chip::app::Metadata;
using namespace chip::app::DataModel;
using namespace chip::app::Clusters;

namespace {

// A fake cluster composition based on GeneralCommissioning ids
namespace ExampleClusterOne {

using namespace chip::app::Clusters::GeneralCommissioning::Attributes;
using namespace chip::app::Clusters::GeneralCommissioning::Commands;

constexpr AttributeMeta kAttributes[] = {
    { Breadcrumb::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View | AttributePrivilege::kWrite_Administer },
    { BasicCommissioningInfo::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { RegulatoryConfig::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { LocationCapability::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { SupportsConcurrentConnection::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },

    // globals
    { GeneratedCommandList::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { AcceptedCommandList::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { AttributeList::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { FeatureMap::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { ClusterRevision::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
};

constexpr CommandMeta kAccepted[] = {
    { ArmFailSafe::Id, BitFlags<CommandQualityFlags>(), Privilege::kAdminister },
    { SetRegulatoryConfig::Id, BitFlags<CommandQualityFlags>(), Privilege::kAdminister },
    { CommissioningComplete::Id, BitFlags<CommandQualityFlags>(CommandQualityFlags::kFabricScoped), Privilege::kAdminister },
};

constexpr CommandId kGenerated[] = { ArmFailSafeResponse::Id, SetRegulatoryConfigResponse::Id, CommissioningCompleteResponse::Id };

constexpr ClusterMeta kMeta = {
    .attributes        = Span<const AttributeMeta>(kAttributes),
    .acceptedCommands  = Span<const CommandMeta>(kAccepted),
    .generatedCommands = Span<const CommandId>(kGenerated),
};

} // namespace ExampleClusterOne

// A fake cluster composition based on UnitTesting cluster IDs
namespace ExampleClusterTwo {

using namespace chip::app::Clusters::UnitTesting::Attributes;
using namespace chip::app::Clusters::UnitTesting::Commands;

constexpr AttributeMeta kAttributes[] = {
    { Boolean::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View | AttributePrivilege::kWrite_Administer },
    { Bitmap8::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { OctetString::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { ListOctetString::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { GlobalEnum::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },

    // globals
    { GeneratedCommandList::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { AcceptedCommandList::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { AttributeList::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { FeatureMap::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
    { ClusterRevision::Id, BitFlags<AttributeQualityFlags>(), AttributePrivilege::kRead_View },
};

constexpr CommandMeta kAccepted[] = {
    { Test::Id, BitFlags<CommandQualityFlags>(), Privilege::kOperate },
    { TestNotHandled::Id, BitFlags<CommandQualityFlags>(), Privilege::kOperate },
    { TestSpecific::Id, BitFlags<CommandQualityFlags>(), Privilege::kOperate },
    { TestSimpleArgumentRequest::Id, BitFlags<CommandQualityFlags>(), Privilege::kOperate },
    { TestAddArguments::Id, BitFlags<CommandQualityFlags>(), Privilege::kOperate },
};

constexpr CommandId kGenerated[] = { TestSpecificResponse::Id, TestSimpleArgumentResponse::Id, TestAddArgumentsResponse::Id };

constexpr ClusterMeta kMeta = {
    .attributes        = Span<const AttributeMeta>(kAttributes),
    .acceptedCommands  = Span<const CommandMeta>(kAccepted),
    .generatedCommands = Span<const CommandId>(kGenerated),
};

} // namespace ExampleClusterTwo

const DataModel::DeviceTypeEntry ep0DeviceTypes[] = { { .deviceTypeId = 0, .deviceTypeRevision = 1 } };

ClusterInstance ep0Clusters[] = { {
                                      .dataVersion      = 0,
                                      .metadata         = &ExampleClusterOne::kMeta,
                                      .attributeHandler = nullptr,
                                      .commandHandler   = nullptr,
                                  },
                                  {
                                      .dataVersion      = 0,
                                      .metadata         = &ExampleClusterTwo::kMeta,
                                      .attributeHandler = nullptr,
                                      .commandHandler   = nullptr,
                                  } };

const DataModel::DeviceTypeEntry ep1DeviceTypes[] = { { .deviceTypeId = 0, .deviceTypeRevision = 1 } };
ClusterInstance ep1Clusters[]                     = { {
                        .dataVersion      = 0,
                        .metadata         = &ExampleClusterTwo::kMeta,
                        .attributeHandler = nullptr,
                        .commandHandler   = nullptr,
} };

EndpointInstance endpoints[] = {
    {
        .id                  = 0,
        .deviceTypes         = Span<const DataModel::DeviceTypeEntry>(ep0DeviceTypes),
        .semanticTags        = Span<const EndpointInstance::SemanticTag>(),
        .serverClusters      = Span<ClusterInstance>(ep0Clusters),
        .clientClusters      = Span<const ClusterId>(),
        .parentEndpointId    = kInvalidEndpointId,
        .endpointComposition = DataModel::EndpointCompositionPattern::kTree,
    },
    {
        .id                  = 1,
        .deviceTypes         = Span<const DataModel::DeviceTypeEntry>(ep1DeviceTypes),
        .semanticTags        = Span<const EndpointInstance::SemanticTag>(),
        .serverClusters      = Span<ClusterInstance>(ep1Clusters),
        .clientClusters      = Span<const ClusterId>(),
        .parentEndpointId    = 0,
        .endpointComposition = DataModel::EndpointCompositionPattern::kTree,
    },
};

} // namespace

TEST(TestMetadataTree, TestEndpointIteration)
{
    CodeMetadataTree tree((Span<EndpointInstance>(endpoints)));

    /// we encode 2 endpoints here
    EXPECT_EQ(tree.FirstEndpoint().id, 0u);
    EXPECT_EQ(tree.NextEndpoint(0).id, 1u);
    EXPECT_EQ(tree.NextEndpoint(1).id, kInvalidEndpointId);

    /// out of order iteration should work
    EXPECT_EQ(tree.NextEndpoint(0).id, 1u);
    EXPECT_EQ(tree.NextEndpoint(0).id, 1u);
    EXPECT_EQ(tree.FirstEndpoint().id, 0u);
    EXPECT_EQ(tree.FirstEndpoint().id, 0u);
    EXPECT_EQ(tree.NextEndpoint(1).id, kInvalidEndpointId);
    EXPECT_EQ(tree.FirstEndpoint().id, 0u);
    EXPECT_EQ(tree.NextEndpoint(1).id, kInvalidEndpointId);
    EXPECT_EQ(tree.NextEndpoint(1).id, kInvalidEndpointId);
    EXPECT_EQ(tree.FirstEndpoint().id, 0u);
    EXPECT_EQ(tree.FirstEndpoint().id, 0u);
}

TEST(TestMetadataTree, TestEndpointInfo)
{
    CodeMetadataTree tree((Span<EndpointInstance>(endpoints)));

    // test that next has ok values
    {
        auto value = tree.FirstEndpoint();
        EXPECT_EQ(value.id, 0);
        EXPECT_EQ(value.info.compositionPattern, EndpointCompositionPattern::kTree);
        EXPECT_EQ(value.info.parentId, kInvalidEndpointId);
    }

    {
        auto value = tree.NextEndpoint(0);
        EXPECT_EQ(value.id, 1);
        EXPECT_EQ(value.info.compositionPattern, EndpointCompositionPattern::kTree);
        EXPECT_EQ(value.info.parentId, 0);
    }

    // direct fetch should work
    {
        auto value = tree.GetEndpointInfo(0);
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value->compositionPattern, EndpointCompositionPattern::kTree);
        EXPECT_EQ(value->parentId, kInvalidEndpointId);
    }
    {
        auto value = tree.GetEndpointInfo(1);
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value->compositionPattern, EndpointCompositionPattern::kTree);
        EXPECT_EQ(value->parentId, 0);
    }

    ASSERT_FALSE(tree.GetEndpointInfo(2).has_value());
    ASSERT_FALSE(tree.GetEndpointInfo(100).has_value());
    ASSERT_FALSE(tree.GetEndpointInfo(1234).has_value());
    ASSERT_FALSE(tree.GetEndpointInfo(0xFFFE).has_value());
    ASSERT_FALSE(tree.GetEndpointInfo(kInvalidEndpointId).has_value());
}
