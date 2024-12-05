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
#include "app/ConcreteAttributePath.h"
#include "app/ConcreteCommandPath.h"
#include <optional>
#include <pw_unit_test/framework.h>

#include <access/Privilege.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app-common/zap-generated/ids/Commands.h>
#include <app/ConcreteClusterPath.h>
#include <app/code-data-model-provider/Metadata.h>
#include <app/code-data-model-provider/MetadataTree.h>
#include <app/data-model-provider/MetadataTypes.h>
#include <app/data-model/Nullable.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/Optional.h>

using namespace chip;
using namespace chip::app;
using namespace chip::Access;
using namespace chip::app::Metadata;
using namespace chip::app::DataModel;
using namespace chip::app::Clusters;

namespace {

// A fake cluster composition based on GeneralCommissioning ids
namespace ExampleClusterOne {

using namespace GeneralCommissioning::Attributes;
using namespace GeneralCommissioning::Commands;

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
    .clusterId         = GeneralCommissioning::Id,
    .qualities         = BitFlags<DataModel::ClusterQualityFlags>(),
    .attributes        = Span<const AttributeMeta>(kAttributes),
    .acceptedCommands  = Span<const CommandMeta>(kAccepted),
    .generatedCommands = Span<const CommandId>(kGenerated),
};

} // namespace ExampleClusterOne

// A fake cluster composition based on UnitTesting cluster IDs
namespace ExampleClusterTwo {

using namespace UnitTesting::Attributes;
using namespace UnitTesting::Commands;

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
    .clusterId         = UnitTesting::Id,
    .qualities         = BitFlags<DataModel::ClusterQualityFlags>(),
    .attributes        = Span<const AttributeMeta>(kAttributes),
    .acceptedCommands  = Span<const CommandMeta>(kAccepted),
    .generatedCommands = Span<const CommandId>(kGenerated),
};

} // namespace ExampleClusterTwo

constexpr DeviceTypeId kRootNodeDeviceType         = 22;
constexpr DeviceTypeId kOnOffLightSwitchDeviceType = 259;
constexpr DeviceTypeId kDimmerSwitchDeviceType     = 260;

const EndpointInstance::SemanticTag kSomeSemanticTags[] = {
    {
        .namespaceID = 1,
        .tag         = 2,
        .label       = MakeOptional(MakeNullable("test"_span)),
    },
    {
        .namespaceID = 123,
        .tag         = 234,
    },
    {
        .namespaceID = 100,
        .tag         = 200,
        .label       = MakeOptional(MakeNullable("foo"_span)),
    },
};

const DataModel::DeviceTypeEntry ep0DeviceTypes[] = { { .deviceTypeId = kRootNodeDeviceType, .deviceTypeRevision = 1 } };

constexpr DataVersion kVer0 = 123;
constexpr DataVersion kVer1 = 2222;

ClusterInstance ep0Clusters[] = { {
                                      .dataVersion      = kVer0,
                                      .metadata         = &ExampleClusterOne::kMeta,
                                      .attributeHandler = nullptr,
                                      .commandHandler   = nullptr,
                                  },
                                  {
                                      .dataVersion      = kVer1,
                                      .metadata         = &ExampleClusterTwo::kMeta,
                                      .attributeHandler = nullptr,
                                      .commandHandler   = nullptr,
                                  } };

const DataModel::DeviceTypeEntry ep1DeviceTypes[] = { { .deviceTypeId = kOnOffLightSwitchDeviceType, .deviceTypeRevision = 1 },
                                                      { .deviceTypeId = kDimmerSwitchDeviceType, .deviceTypeRevision = 1 } };

constexpr DataVersion kVer2   = 234;
ClusterInstance ep1Clusters[] = { {
    .dataVersion      = kVer2,
    .metadata         = &ExampleClusterTwo::kMeta,
    .attributeHandler = nullptr,
    .commandHandler   = nullptr,
} };

const ClusterId kSomeClientClusters[] = {
    Binding::Id,
    Descriptor::Id,
};

EndpointInstance endpoints[] = {
    {
        .id                  = 0,
        .deviceTypes         = Span<const DataModel::DeviceTypeEntry>(ep0DeviceTypes),
        .semanticTags        = Span<const EndpointInstance::SemanticTag>(),
        .serverClusters      = Span<ClusterInstance>(ep0Clusters),
        .clientClusters      = Span<const ClusterId>(kSomeClientClusters),
        .parentEndpointId    = kInvalidEndpointId,
        .endpointComposition = DataModel::EndpointCompositionPattern::kTree,
    },
    {
        .id                  = 1,
        .deviceTypes         = Span<const DataModel::DeviceTypeEntry>(ep1DeviceTypes),
        .semanticTags        = Span<const EndpointInstance::SemanticTag>(kSomeSemanticTags),
        .serverClusters      = Span<ClusterInstance>(ep1Clusters),
        .clientClusters      = Span<const ClusterId>(),
        .parentEndpointId    = 0,
        .endpointComposition = DataModel::EndpointCompositionPattern::kTree,
    },
};

} // namespace
  //
TEST(TestMetadataTree, TestEmptyTree)
{
    CodeMetadataTree tree((Span<EndpointInstance>()));

    EXPECT_FALSE(tree.FirstEndpoint().IsValid());
    EXPECT_FALSE(tree.NextEndpoint(0).IsValid());
    EXPECT_FALSE(tree.NextEndpoint(kInvalidEndpointId).IsValid());
    EXPECT_FALSE(tree.GetEndpointInfo(0).has_value());
    EXPECT_FALSE(tree.GetEndpointInfo(1).has_value());
    EXPECT_FALSE(tree.GetEndpointInfo(kInvalidEndpointId).has_value());
    EXPECT_FALSE(tree.FirstDeviceType(0).has_value());
    EXPECT_FALSE(tree.NextDeviceType(0, {}).has_value());
    EXPECT_FALSE(tree.GetFirstSemanticTag(0).has_value());
    EXPECT_FALSE(tree.GetNextSemanticTag(123, {}).has_value());
}

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

    // invalid should work
    EXPECT_FALSE(tree.NextEndpoint(2).IsValid());
    EXPECT_FALSE(tree.NextEndpoint(100).IsValid());
    EXPECT_FALSE(tree.NextEndpoint(1234).IsValid());
    EXPECT_FALSE(tree.NextEndpoint(0xFFFE).IsValid());
    EXPECT_FALSE(tree.NextEndpoint(kInvalidEndpointId).IsValid());
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

    EXPECT_FALSE(tree.GetEndpointInfo(2).has_value());
    EXPECT_FALSE(tree.GetEndpointInfo(100).has_value());
    EXPECT_FALSE(tree.GetEndpointInfo(1234).has_value());
    EXPECT_FALSE(tree.GetEndpointInfo(0xFFFE).has_value());
    EXPECT_FALSE(tree.GetEndpointInfo(kInvalidEndpointId).has_value());
}

TEST(TestMetadataTree, TestDeviceTypes)
{
    CodeMetadataTree tree((Span<EndpointInstance>(endpoints)));

    {
        auto value = tree.FirstDeviceType(0);
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value->deviceTypeId, kRootNodeDeviceType);
        EXPECT_EQ(value->deviceTypeRevision, 1);

        value = tree.NextDeviceType(0, *value);
        EXPECT_FALSE(value.has_value());
    }

    {
        auto value = tree.FirstDeviceType(1);
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value->deviceTypeId, kOnOffLightSwitchDeviceType);
        EXPECT_EQ(value->deviceTypeRevision, 1);

        value = tree.NextDeviceType(1, *value);
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value->deviceTypeId, kDimmerSwitchDeviceType);
        EXPECT_EQ(value->deviceTypeRevision, 1);

        value = tree.NextDeviceType(1, *value);
        EXPECT_FALSE(value.has_value());
    }

    {
        // can run the same query several times
        auto value = tree.FirstDeviceType(1);
        ASSERT_TRUE(value.has_value());

        EXPECT_TRUE(tree.NextDeviceType(1, *value).has_value());
        EXPECT_TRUE(tree.NextDeviceType(1, *value).has_value());
        EXPECT_TRUE(tree.NextDeviceType(1, *value).has_value());
        EXPECT_TRUE(tree.NextDeviceType(1, *value).has_value());
    }

    EXPECT_FALSE(tree.FirstDeviceType(2).has_value());
    EXPECT_FALSE(tree.FirstDeviceType(100).has_value());
    EXPECT_FALSE(tree.FirstDeviceType(123).has_value());
    EXPECT_FALSE(tree.FirstDeviceType(0xFFFE).has_value());
    EXPECT_FALSE(tree.FirstDeviceType(kInvalidEndpointId).has_value());

    // invalid entries
    EXPECT_FALSE(tree.NextDeviceType(0, DeviceTypeEntry{}).has_value());
    EXPECT_FALSE(tree.NextDeviceType(1, DeviceTypeEntry{}).has_value());

    // some nonsense queries of invalid indexes
    EXPECT_FALSE(tree.NextDeviceType(2, DeviceTypeEntry{}).has_value());
    EXPECT_FALSE(tree.NextDeviceType(123, DeviceTypeEntry{}).has_value());
    EXPECT_FALSE(tree.NextDeviceType(0xFFFE, DeviceTypeEntry{}).has_value());
    EXPECT_FALSE(tree.NextDeviceType(kInvalidEndpointId, DeviceTypeEntry{}).has_value());
}

TEST(TestMetadataTree, TestSemanticTags)
{
    CodeMetadataTree tree((Span<EndpointInstance>(endpoints)));

    // no semantic tags set on root
    EXPECT_FALSE(tree.GetFirstSemanticTag(0).has_value());

    // EP1 semantic tags test
    {
        auto value = tree.GetFirstSemanticTag(1);
        ASSERT_TRUE(value.has_value());

        EXPECT_EQ(value->namespaceID, kSomeSemanticTags[0].namespaceID);
        EXPECT_EQ(value->tag, kSomeSemanticTags[0].tag);

        // first semantic tag has a label, so test that. This optional nullable is messy
        ASSERT_TRUE(value->label.HasValue());
        ASSERT_FALSE(value->label.Value().IsNull());
        ASSERT_TRUE(value->label.Value().Value().data_equal(kSomeSemanticTags[0].label.Value().Value()));

        for (unsigned i = 1; i < ArraySize(kSomeSemanticTags); i++)
        {
            value = tree.GetNextSemanticTag(1, *value);
            ASSERT_TRUE(value.has_value());

            EXPECT_EQ(value->namespaceID, kSomeSemanticTags[i].namespaceID);
            EXPECT_EQ(value->tag, kSomeSemanticTags[i].tag);
        }

        EXPECT_FALSE(tree.GetNextSemanticTag(1, *value).has_value());
    }

    // some repeat calls should work
    {
        auto value = tree.GetFirstSemanticTag(1);
        ASSERT_TRUE(value.has_value());

        EXPECT_TRUE(tree.GetNextSemanticTag(1, *value).has_value());
        EXPECT_TRUE(tree.GetNextSemanticTag(1, *value).has_value());
        EXPECT_TRUE(tree.GetNextSemanticTag(1, *value).has_value());

        value = tree.GetNextSemanticTag(1, *value);
        ASSERT_TRUE(value.has_value());

        EXPECT_TRUE(tree.GetNextSemanticTag(1, *value).has_value());
        EXPECT_FALSE(tree.GetNextSemanticTag(0, *value).has_value());
        EXPECT_TRUE(tree.GetNextSemanticTag(1, *value).has_value());
        EXPECT_FALSE(tree.GetNextSemanticTag(0, *value).has_value());
        EXPECT_TRUE(tree.GetNextSemanticTag(1, *value).has_value());
    }
    // these are not tags on EP0 or 1
    EXPECT_FALSE(tree.GetNextSemanticTag(0, EndpointInstance::SemanticTag{}).has_value());
    EXPECT_FALSE(tree.GetNextSemanticTag(1, EndpointInstance::SemanticTag{}).has_value());

    // invalid getters
    EXPECT_FALSE(tree.GetFirstSemanticTag(2).has_value());
    EXPECT_FALSE(tree.GetFirstSemanticTag(100).has_value());
    EXPECT_FALSE(tree.GetFirstSemanticTag(0xFFFE).has_value());
    EXPECT_FALSE(tree.GetFirstSemanticTag(kInvalidEndpointId).has_value());

    // next called on invalid endpoint
    EXPECT_FALSE(tree.GetNextSemanticTag(2, EndpointInstance::SemanticTag{}).has_value());
    EXPECT_FALSE(tree.GetNextSemanticTag(0xFFFE, EndpointInstance::SemanticTag{}).has_value());
}

TEST(TestMetadataTree, TestServerClusterIteration)
{
    CodeMetadataTree tree((Span<EndpointInstance>(endpoints)));

    {
        auto value = tree.FirstServerCluster(0);
        EXPECT_TRUE(value.IsValid());
        EXPECT_EQ(value.path, ConcreteClusterPath(0, GeneralCommissioning::Id));
        EXPECT_EQ(value.info.dataVersion, kVer0);

        value = tree.NextServerCluster(value.path);
        EXPECT_TRUE(value.IsValid());
        EXPECT_EQ(value.path, ConcreteClusterPath(0, UnitTesting::Id));
        EXPECT_EQ(value.info.dataVersion, kVer1);

        value = tree.NextServerCluster(value.path);
        EXPECT_FALSE(value.IsValid());
    }
    {

        auto value = tree.FirstServerCluster(1);
        EXPECT_TRUE(value.IsValid());
        EXPECT_EQ(value.path, ConcreteClusterPath(1, UnitTesting::Id));
        EXPECT_EQ(value.info.dataVersion, kVer2);

        value = tree.NextServerCluster(value.path);
        EXPECT_FALSE(value.IsValid());
    }

    EXPECT_FALSE(tree.FirstServerCluster(2).IsValid());
    EXPECT_FALSE(tree.FirstServerCluster(123).IsValid());
    EXPECT_FALSE(tree.FirstServerCluster(0xFFFE).IsValid());
    EXPECT_FALSE(tree.FirstServerCluster(kInvalidEndpointId).IsValid());

    EXPECT_FALSE(tree.NextServerCluster(ConcreteClusterPath(kInvalidEndpointId, 1)).IsValid());
    EXPECT_FALSE(tree.NextServerCluster(ConcreteClusterPath(2, 0)).IsValid());
}

TEST(TestMetadataTree, TestServerClusterInfo)
{
    CodeMetadataTree tree((Span<EndpointInstance>(endpoints)));

    auto value = tree.GetServerClusterInfo(ConcreteClusterPath(0, GeneralCommissioning::Id));
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->dataVersion, kVer0);

    // repeat call should be ok
    value = tree.GetServerClusterInfo(ConcreteClusterPath(0, GeneralCommissioning::Id));
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->dataVersion, kVer0);

    value = tree.GetServerClusterInfo(ConcreteClusterPath(0, UnitTesting::Id));
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->dataVersion, kVer1);

    value = tree.GetServerClusterInfo(ConcreteClusterPath(1, UnitTesting::Id));
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(value->dataVersion, kVer2);

    ASSERT_FALSE(tree.GetServerClusterInfo(ConcreteClusterPath(2, UnitTesting::Id)).has_value());
    ASSERT_FALSE(tree.GetServerClusterInfo(ConcreteClusterPath(123, UnitTesting::Id)).has_value());
    ASSERT_FALSE(tree.GetServerClusterInfo(ConcreteClusterPath(0xFFFE, PowerSource::Id)).has_value());
    ASSERT_FALSE(tree.GetServerClusterInfo(ConcreteClusterPath(1, AccessControl::Id)).has_value());
    ASSERT_FALSE(tree.GetServerClusterInfo(ConcreteClusterPath(100, AccessControl::Id)).has_value());
}

TEST(TestMetadataTree, TestClientClustersIteration)
{
    CodeMetadataTree tree((Span<EndpointInstance>(endpoints)));

    {
        auto value = tree.FirstClientCluster(0);
        EXPECT_EQ(value, ConcreteClusterPath(0, Binding::Id));

        value = tree.NextClientCluster(value);
        EXPECT_EQ(value, ConcreteClusterPath(0, Descriptor::Id));

        value = tree.NextClientCluster(value);
        EXPECT_EQ(value, ConcreteClusterPath{});
    }

    EXPECT_EQ(tree.FirstClientCluster(1), ConcreteClusterPath{});
    EXPECT_EQ(tree.FirstClientCluster(2), ConcreteClusterPath{});
    EXPECT_EQ(tree.FirstClientCluster(100), ConcreteClusterPath{});
    EXPECT_EQ(tree.FirstClientCluster(0xFFFE), ConcreteClusterPath{});
    EXPECT_EQ(tree.FirstClientCluster(kInvalidEndpointId), ConcreteClusterPath{});

    EXPECT_EQ(tree.NextClientCluster({ kInvalidEndpointId, 123 }), ConcreteClusterPath{});
    EXPECT_EQ(tree.NextClientCluster({ kInvalidEndpointId, Binding::Id }), ConcreteClusterPath{});
    EXPECT_EQ(tree.NextClientCluster({ 100, Binding::Id }), ConcreteClusterPath{});
}

TEST(TestMetadataTree, TestAttributeIteration)
{
    CodeMetadataTree tree((Span<EndpointInstance>(endpoints)));

    {
        auto value = tree.FirstAttribute({ 1, UnitTesting::Id });

        // iteration over attributes should be identical with what we have in the metadata
        for (auto & attr : ExampleClusterTwo::kAttributes)
        {
            ASSERT_TRUE(value.IsValid());
            EXPECT_EQ(value.path, ConcreteAttributePath(1, UnitTesting::Id, attr.id));
            EXPECT_EQ(value.info.flags, attr.qualities);
            EXPECT_EQ(value.info.readPrivilege, ReadPrivilege(attr.privileges));
            EXPECT_EQ(value.info.writePrivilege, WritePrivilege(attr.privileges));
            value = tree.NextAttribute(value.path);
        }
        ASSERT_FALSE(value.IsValid());

        value = tree.FirstAttribute({ 0, GeneralCommissioning::Id });

        // iteration over attributes should be identical with what we have in the metadata
        for (auto & attr : ExampleClusterOne::kAttributes)
        {
            ASSERT_TRUE(value.IsValid());
            EXPECT_EQ(value.path, ConcreteAttributePath(0, GeneralCommissioning::Id, attr.id));
            EXPECT_EQ(value.info.flags, attr.qualities);
            EXPECT_EQ(value.info.readPrivilege, ReadPrivilege(attr.privileges));
            EXPECT_EQ(value.info.writePrivilege, WritePrivilege(attr.privileges));
            value = tree.NextAttribute(value.path);
        }
        ASSERT_FALSE(value.IsValid());
    }

    {
        /// repeat fetching should be ok
        ASSERT_TRUE(tree.FirstAttribute({ 0, GeneralCommissioning::Id }).IsValid());
        ASSERT_TRUE(tree.FirstAttribute({ 0, GeneralCommissioning::Id }).IsValid());
        ASSERT_TRUE(tree.FirstAttribute({ 0, GeneralCommissioning::Id }).IsValid());
        ASSERT_TRUE(tree.FirstAttribute({ 0, GeneralCommissioning::Id }).IsValid());

        ASSERT_TRUE(tree.NextAttribute({ 0, GeneralCommissioning::Id, GeneralCommissioning::Attributes::LocationCapability::Id })
                        .IsValid());
        ASSERT_TRUE(tree.NextAttribute({ 0, GeneralCommissioning::Id, GeneralCommissioning::Attributes::LocationCapability::Id })
                        .IsValid());
        ASSERT_TRUE(tree.NextAttribute({ 0, GeneralCommissioning::Id, GeneralCommissioning::Attributes::LocationCapability::Id })
                        .IsValid());
        ASSERT_TRUE(tree.NextAttribute({ 0, GeneralCommissioning::Id, GeneralCommissioning::Attributes::LocationCapability::Id })
                        .IsValid());
        ASSERT_TRUE(tree.NextAttribute({ 0, GeneralCommissioning::Id, GeneralCommissioning::Attributes::LocationCapability::Id })
                        .IsValid());
        ASSERT_TRUE(
            tree.NextAttribute({ 0, GeneralCommissioning::Id, GeneralCommissioning::Attributes::Breadcrumb::Id }).IsValid());
        ASSERT_TRUE(
            tree.NextAttribute({ 0, GeneralCommissioning::Id, GeneralCommissioning::Attributes::Breadcrumb::Id }).IsValid());
        ASSERT_TRUE(tree.NextAttribute({ 0, GeneralCommissioning::Id, GeneralCommissioning::Attributes::LocationCapability::Id })
                        .IsValid());
        ASSERT_TRUE(
            tree.NextAttribute({ 0, GeneralCommissioning::Id, GeneralCommissioning::Attributes::Breadcrumb::Id }).IsValid());
    }

    // Some invalid fetches
    EXPECT_FALSE(tree.FirstAttribute({ 1, GeneralCommissioning::Id }).IsValid());
    EXPECT_FALSE(tree.FirstAttribute({ 100, GeneralCommissioning::Id }).IsValid());
    EXPECT_FALSE(tree.FirstAttribute({ kInvalidEndpointId, UnitTesting::Id }).IsValid());

    EXPECT_FALSE(tree.NextAttribute({ 1, GeneralCommissioning::Id, 0 }).IsValid());
    EXPECT_FALSE(tree.NextAttribute({ 100, GeneralCommissioning::Id, 1 }).IsValid());
    EXPECT_FALSE(tree.NextAttribute({ 0, GeneralCommissioning::Id, kInvalidAttributeId }).IsValid());
    EXPECT_FALSE(tree.NextAttribute({ 0, GeneralCommissioning::Id, 0xBADBAD }).IsValid());
    EXPECT_FALSE(tree.NextAttribute({ kInvalidEndpointId, UnitTesting::Id, 100 }).IsValid());
}

TEST(TestMetadataTree, TestAttributeInfo)
{
    CodeMetadataTree tree((Span<EndpointInstance>(endpoints)));

    // iteration over attributes should be identical with what we have in the metadata
    for (auto & attr : ExampleClusterTwo::kAttributes)
    {
        auto value = tree.GetAttributeInfo({ 1, UnitTesting::Id, attr.id });
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value->flags, attr.qualities);
        EXPECT_EQ(value->readPrivilege, ReadPrivilege(attr.privileges));
        EXPECT_EQ(value->writePrivilege, WritePrivilege(attr.privileges));
    }

    // invalid attributes should not have info
    EXPECT_FALSE(tree.GetAttributeInfo({ 1, GeneralCommissioning::Id, 0 }).has_value());
    EXPECT_FALSE(tree.GetAttributeInfo({ 100, GeneralCommissioning::Id, 1 }).has_value());
    EXPECT_FALSE(tree.GetAttributeInfo({ 0, GeneralCommissioning::Id, kInvalidAttributeId }).has_value());
    EXPECT_FALSE(tree.GetAttributeInfo({ 0, GeneralCommissioning::Id, 0xBADBAD }).has_value());
    EXPECT_FALSE(tree.GetAttributeInfo({ kInvalidEndpointId, UnitTesting::Id, 100 }).has_value());
}

TEST(TestMetadataTree, TestGeneratedCommandsIteration)
{
    CodeMetadataTree tree((Span<EndpointInstance>(endpoints)));

    {
        ConcreteCommandPath path = tree.FirstGeneratedCommand({0, GeneralCommissioning::Id});

        for (auto &id : ExampleClusterOne::kGenerated) {
            EXPECT_EQ(path, ConcreteCommandPath(0, GeneralCommissioning::Id, id));
            path = tree.NextGeneratedCommand(path);
        }
    }

    // some nonsense paths
    ASSERT_FALSE(tree.FirstGeneratedCommand({0, PowerSource::Id}).HasValidIds());
    ASSERT_FALSE(tree.FirstGeneratedCommand({123, GeneralCommissioning::Id}).HasValidIds());
    ASSERT_FALSE(tree.FirstGeneratedCommand({kInvalidEndpointId, GeneralCommissioning::Id}).HasValidIds());
    ASSERT_FALSE(tree.FirstGeneratedCommand({0, kInvalidCommandId}).HasValidIds());

    ASSERT_FALSE(tree.NextGeneratedCommand({0, GeneralCommissioning::Id, kInvalidCommandId}).HasValidIds());
    ASSERT_FALSE(tree.NextGeneratedCommand({0, GeneralCommissioning::Id, 0x123FEFE}).HasValidIds());
    ASSERT_FALSE(tree.NextGeneratedCommand({123, GeneralCommissioning::Id, GeneralCommissioning::Commands::ArmFailSafe::Id}).HasValidIds());
    ASSERT_FALSE(tree.NextGeneratedCommand({kInvalidEndpointId, GeneralCommissioning::Id, GeneralCommissioning::Commands::ArmFailSafe::Id}).HasValidIds());
    ASSERT_FALSE(tree.NextGeneratedCommand({0, kInvalidClusterId, GeneralCommissioning::Commands::ArmFailSafe::Id}).HasValidIds());
}
