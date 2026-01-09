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

#include <app/clusters/on-off-server/OnOffCluster.h>
#include <clusters/OnOff/Metadata.h>
#include <pw_unit_test/framework.h>

#include <app/DefaultSafeAttributePersistenceProvider.h>
#include <app/SafeAttributePersistenceProvider.h>
#include <app/server-cluster/testing/AttributeTesting.h>
#include <app/server-cluster/testing/ClusterTester.h>
#include <app/server-cluster/testing/TestServerClusterContext.h>
#include <app/server-cluster/testing/ValidateGlobalAttributes.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::OnOff;
using namespace chip::Testing;

using chip::Testing::IsAcceptedCommandsListEqualTo;
using chip::Testing::IsAttributesListEqualTo;

namespace {

constexpr EndpointId kTestEndpointId = 1;

class MockOnOffDelegate : public OnOffDelegate
{
public:
    bool mOnOff         = false;
    bool mCalled        = false;
    bool mStartupCalled = false;

    void OnOnOffChanged(bool on) override
    {
        mOnOff  = on;
        mCalled = true;
    }

    void OnOffStartup(bool on) override
    {
        mOnOff         = on;
        mStartupCalled = true;
    }
};

struct TestOnOffCluster : public ::testing::Test
{
    static void SetUpTestSuite() { ASSERT_EQ(Platform::MemoryInit(), CHIP_NO_ERROR); }
    static void TearDownTestSuite() { Platform::MemoryShutdown(); }

    void SetUp() override
    {
        VerifyOrDie(mPersistenceProvider.Init(&mClusterTester.GetServerClusterContext().storage) == CHIP_NO_ERROR);
        app::SetSafeAttributePersistenceProvider(&mPersistenceProvider);
        mCluster.AddDelegate(&mMockDelegate);
        EXPECT_EQ(mCluster.Startup(mClusterTester.GetServerClusterContext()), CHIP_NO_ERROR);
    }

    void TearDown() override { app::SetSafeAttributePersistenceProvider(nullptr); }

    MockOnOffDelegate mMockDelegate;

    OnOffCluster mCluster{ kTestEndpointId };

    ClusterTester mClusterTester{ mCluster };

    app::DefaultSafeAttributePersistenceProvider mPersistenceProvider;
};

TEST_F(TestOnOffCluster, TestAttributesList)
{
    std::vector<DataModel::AttributeEntry> mandatoryAttributes(Attributes::kMandatoryMetadata.begin(),
                                                               Attributes::kMandatoryMetadata.end());
    EXPECT_TRUE(IsAttributesListEqualTo(mCluster, mandatoryAttributes));
}

TEST_F(TestOnOffCluster, TestAcceptedCommands)
{
    EXPECT_TRUE(IsAcceptedCommandsListEqualTo(mCluster,
                                              {
                                                  Commands::Off::kMetadataEntry,
                                                  Commands::On::kMetadataEntry,
                                                  Commands::Toggle::kMetadataEntry,
                                              }));
}

TEST_F(TestOnOffCluster, TestReadAttributes)
{
    // Test Read OnOff (Default false)
    bool onOff = true;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OnOff::Id, onOff), CHIP_NO_ERROR);
    EXPECT_FALSE(onOff);

    // Test Read ClusterRevision
    uint16_t revision = 0;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::ClusterRevision::Id, revision), CHIP_NO_ERROR);
    EXPECT_EQ(revision, kRevision);

    // Test Read FeatureMap
    uint32_t featureMap = 1;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::FeatureMap::Id, featureMap), CHIP_NO_ERROR);
    EXPECT_EQ(featureMap, 0u);
}

TEST_F(TestOnOffCluster, TestCommands)
{
    // 1. On Command
    EXPECT_TRUE(mClusterTester.Invoke<Commands::On::Type>(Commands::On::Type()).IsSuccess());
    EXPECT_TRUE(mMockDelegate.mCalled);
    EXPECT_TRUE(mMockDelegate.mOnOff);
    mMockDelegate.mCalled = false;

    // Verify Attribute
    bool onOff = false;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OnOff::Id, onOff), CHIP_NO_ERROR);
    EXPECT_TRUE(onOff);

    // 2. Off Command
    EXPECT_TRUE(mClusterTester.Invoke<Commands::Off::Type>(Commands::Off::Type()).IsSuccess());
    EXPECT_TRUE(mMockDelegate.mCalled);
    EXPECT_FALSE(mMockDelegate.mOnOff);
    mMockDelegate.mCalled = false;

    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OnOff::Id, onOff), CHIP_NO_ERROR);
    EXPECT_FALSE(onOff);

    // 3. Toggle Command (from Off to On)
    EXPECT_TRUE(mClusterTester.Invoke<Commands::Toggle::Type>(Commands::Toggle::Type()).IsSuccess());
    EXPECT_TRUE(mMockDelegate.mCalled);
    EXPECT_TRUE(mMockDelegate.mOnOff);
    mMockDelegate.mCalled = false;

    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OnOff::Id, onOff), CHIP_NO_ERROR);
    EXPECT_TRUE(onOff);

    // 4. Toggle Command (from On to Off)
    EXPECT_TRUE(mClusterTester.Invoke<Commands::Toggle::Type>(Commands::Toggle::Type()).IsSuccess());
    EXPECT_TRUE(mMockDelegate.mCalled);
    EXPECT_FALSE(mMockDelegate.mOnOff);

    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OnOff::Id, onOff), CHIP_NO_ERROR);
    EXPECT_FALSE(onOff);
}

TEST_F(TestOnOffCluster, TestPersistence)
{
    TestServerClusterContext context;
    app::DefaultSafeAttributePersistenceProvider persistenceProvider;
    EXPECT_EQ(persistenceProvider.Init(&context.Get().storage), CHIP_NO_ERROR);
    app::SetSafeAttributePersistenceProvider(&persistenceProvider);
    MockOnOffDelegate mockDelegate;

    // 1. Initial startup, set to ON
    {
        OnOffCluster cluster(kTestEndpointId);
        cluster.AddDelegate(&mockDelegate);
        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);
        chip::Testing::ClusterTester tester(cluster);

        EXPECT_TRUE(tester.Invoke<Commands::On::Type>(Commands::On::Type()).IsSuccess());
        bool onOff = false;
        EXPECT_EQ(tester.ReadAttribute(Attributes::OnOff::Id, onOff), CHIP_NO_ERROR);
        EXPECT_TRUE(onOff);
    }

    // 2. Restart, verify ON
    {
        OnOffCluster cluster(kTestEndpointId);
        cluster.AddDelegate(&mockDelegate);
        EXPECT_EQ(cluster.Startup(context.Get()), CHIP_NO_ERROR);
        chip::Testing::ClusterTester tester(cluster);

        bool onOff = false;
        EXPECT_EQ(tester.ReadAttribute(Attributes::OnOff::Id, onOff), CHIP_NO_ERROR);
        EXPECT_TRUE(onOff);
    }
}

struct TestOffOnlyOnOffCluster : public ::testing::Test
{
    static void SetUpTestSuite() { ASSERT_EQ(Platform::MemoryInit(), CHIP_NO_ERROR); }
    static void TearDownTestSuite() { Platform::MemoryShutdown(); }

    void SetUp() override
    {
        VerifyOrDie(mPersistenceProvider.Init(&mClusterTester.GetServerClusterContext().storage) == CHIP_NO_ERROR);
        app::SetSafeAttributePersistenceProvider(&mPersistenceProvider);
        mCluster.AddDelegate(&mMockDelegate);
        EXPECT_EQ(mCluster.Startup(mClusterTester.GetServerClusterContext()), CHIP_NO_ERROR);
    }

    void TearDown() override { app::SetSafeAttributePersistenceProvider(nullptr); }

    MockOnOffDelegate mMockDelegate;
    OnOffCluster mCluster{ kTestEndpointId, BitMask<Feature>(Feature::kOffOnly) };
    ClusterTester mClusterTester{ mCluster };
    app::DefaultSafeAttributePersistenceProvider mPersistenceProvider;
};

TEST_F(TestOffOnlyOnOffCluster, TestFeatureMap)
{
    uint32_t featureMap = 0;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::FeatureMap::Id, featureMap), CHIP_NO_ERROR);
    EXPECT_EQ(featureMap, static_cast<uint32_t>(Feature::kOffOnly));
}

TEST_F(TestOffOnlyOnOffCluster, TestAcceptedCommands)
{
    EXPECT_TRUE(IsAcceptedCommandsListEqualTo(mCluster, { Commands::Off::kMetadataEntry }));
}

TEST_F(TestOffOnlyOnOffCluster, TestInvokeCommands)
{
    // Off should still work
    EXPECT_TRUE(mClusterTester.Invoke<Commands::Off::Type>(Commands::Off::Type()).IsSuccess());

    // On should fail with UnsupportedCommand
    EXPECT_EQ(mClusterTester.Invoke<Commands::On::Type>(Commands::On::Type()).status,
              Protocols::InteractionModel::Status::UnsupportedCommand);

    // Toggle should fail with UnsupportedCommand
    EXPECT_EQ(mClusterTester.Invoke<Commands::Toggle::Type>(Commands::Toggle::Type()).status,
              Protocols::InteractionModel::Status::UnsupportedCommand);
}

TEST_F(TestOnOffCluster, TestMultipleDelegates)
{
    MockOnOffDelegate secondaryDelegate;
    mCluster.AddDelegate(&secondaryDelegate);

    // 1. On Command - Both should be called
    EXPECT_TRUE(mClusterTester.Invoke<Commands::On::Type>(Commands::On::Type()).IsSuccess());

    EXPECT_TRUE(mMockDelegate.mCalled);
    EXPECT_TRUE(mMockDelegate.mOnOff);
    mMockDelegate.mCalled = false;

    EXPECT_TRUE(secondaryDelegate.mCalled);
    EXPECT_TRUE(secondaryDelegate.mOnOff);
    secondaryDelegate.mCalled = false;

    // 2. Remove secondary delegate
    mCluster.RemoveDelegate(&secondaryDelegate);

    // 3. Off Command - Only primary should be called
    EXPECT_TRUE(mClusterTester.Invoke<Commands::Off::Type>(Commands::Off::Type()).IsSuccess());

    EXPECT_TRUE(mMockDelegate.mCalled);
    EXPECT_FALSE(mMockDelegate.mOnOff);

    EXPECT_FALSE(secondaryDelegate.mCalled);
    EXPECT_TRUE(secondaryDelegate.mOnOff); // Should remain true (last state)
}

TEST_F(TestOnOffCluster, TestSceneSupport)
{
    // 1. Setup: Turn ON
    EXPECT_TRUE(mClusterTester.Invoke<Commands::On::Type>(Commands::On::Type()).IsSuccess());
    EXPECT_TRUE(mMockDelegate.mOnOff);

    // 2. Serialize Save
    uint8_t buffer[128];
    MutableByteSpan serializedBytes(buffer);
    EXPECT_EQ(mCluster.SerializeSave(kTestEndpointId, Clusters::OnOff::Id, serializedBytes), CHIP_NO_ERROR);
    EXPECT_GT(serializedBytes.size(), 0u);

    // 3. Turn OFF
    EXPECT_TRUE(mClusterTester.Invoke<Commands::Off::Type>(Commands::Off::Type()).IsSuccess());
    EXPECT_FALSE(mMockDelegate.mOnOff);

    // 4. Apply Scene (Restore ON)
    EXPECT_EQ(mCluster.ApplyScene(kTestEndpointId, Clusters::OnOff::Id, serializedBytes, 0), CHIP_NO_ERROR);
    EXPECT_TRUE(mMockDelegate.mOnOff);
}

TEST_F(TestOnOffCluster, TestSceneInvalidAttribute)
{
    using AttributeValuePair = ScenesManagement::Structs::AttributeValuePairStruct::Type;

    // Construct invalid data (Wrong Attribute ID)
    AttributeValuePair pairs[1];
    pairs[0].attributeID = Attributes::OnOff::Id + 1; // Invalid ID
    pairs[0].valueUnsigned8.SetValue(1);

    uint8_t buffer[128];
    MutableByteSpan serializedBytes(buffer);
    app::DataModel::List<AttributeValuePair> attributeValueList(pairs);

    // Encode invalid list
    EXPECT_EQ(mCluster.EncodeAttributeValueList(attributeValueList, serializedBytes), CHIP_NO_ERROR);

    // Apply Scene should fail
    EXPECT_EQ(mCluster.ApplyScene(kTestEndpointId, Clusters::OnOff::Id, serializedBytes, 0), CHIP_ERROR_INVALID_ARGUMENT);
}

} // namespace
