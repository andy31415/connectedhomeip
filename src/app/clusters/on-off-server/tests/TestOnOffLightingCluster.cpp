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

#include <app/clusters/on-off-server/OnOffLightingCluster.h>
#include <app/clusters/scenes-server/ScenesIntegrationDelegate.h>
#include <clusters/OnOff/Metadata.h>
#include <lib/support/TimerDelegateMock.h>
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

using chip::Protocols::InteractionModel::Status;
using chip::Testing::IsAcceptedCommandsListEqualTo;
using chip::Testing::IsAttributesListEqualTo;

namespace {

constexpr EndpointId kTestEndpointId = 1;

class MockScenesIntegrationDelegate : public chip::scenes::ScenesIntegrationDelegate
{
public:
    struct Call
    {
        FabricIndex fabricIndex;
    };
    std::vector<Call> storeCalls;
    std::vector<Call> recallCalls;
    int markInvalidCalls        = 0;
    int groupWillBeRemovedCalls = 0;

    CHIP_ERROR GroupWillBeRemoved(FabricIndex fabricIndex, GroupId groupId) override
    {
        groupWillBeRemovedCalls++;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR MakeSceneInvalidForAllFabrics() override
    {
        markInvalidCalls++;
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR StoreCurrentGlobalScene(FabricIndex fabricIndex) override
    {
        storeCalls.push_back({ fabricIndex });
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR RecallGlobalScene(FabricIndex fabricIndex) override
    {
        recallCalls.push_back({ fabricIndex });
        return CHIP_NO_ERROR;
    }
};

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

class MockOnOffEffectDelegate : public OnOffEffectDelegate
{
public:
    EffectIdentifierEnum mEffectId = EffectIdentifierEnum::kDelayedAllOff;
    uint8_t mEffectVariant         = 0;
    bool mCalled                   = false;

    void TriggerEffect(EffectIdentifierEnum effectId, uint8_t effectVariant) override
    {
        mEffectId      = effectId;
        mEffectVariant = effectVariant;
        mCalled        = true;
    }
};

struct TestOnOffLightingCluster : public ::testing::Test
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
    TimerDelegateMock mMockTimerDelegate;
    MockOnOffEffectDelegate mMockEffectDelegate;
    MockScenesIntegrationDelegate mMockScenesIntegrationDelegate;

    OnOffLightingCluster mCluster{ kTestEndpointId, mMockTimerDelegate, mMockEffectDelegate, &mMockScenesIntegrationDelegate };

    ClusterTester mClusterTester{ mCluster };

    app::DefaultSafeAttributePersistenceProvider mPersistenceProvider;
};

TEST_F(TestOnOffLightingCluster, TestLightingAttributes)
{
    // Test Read GlobalSceneControl (Default true)
    bool globalSceneControl = false;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::GlobalSceneControl::Id, globalSceneControl), CHIP_NO_ERROR);
    EXPECT_TRUE(globalSceneControl);

    // Test Read OnTime (Default 0)
    uint16_t onTime = 1;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OnTime::Id, onTime), CHIP_NO_ERROR);
    EXPECT_EQ(onTime, 0);

    // Test Read OffWaitTime (Default 0)
    uint16_t offWaitTime = 1;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OffWaitTime::Id, offWaitTime), CHIP_NO_ERROR);
    EXPECT_EQ(offWaitTime, 0);

    // Test Read StartUpOnOff (Default Null)
    DataModel::Nullable<StartUpOnOffEnum> startUpOnOff;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::StartUpOnOff::Id, startUpOnOff), CHIP_NO_ERROR);
    EXPECT_TRUE(startUpOnOff.IsNull());
}

TEST_F(TestOnOffLightingCluster, TestOnWithTimedOff)
{
    // 1. Turn On with Timed Off (OnTime = 10, OffWaitTime = 20)
    Commands::OnWithTimedOff::Type command;
    command.onOffControl.SetField(OnOffControlBitmap::kAcceptOnlyWhenOn, 0); // Unconditional
    command.onTime      = 10;
    command.offWaitTime = 20;

    EXPECT_TRUE(mClusterTester.Invoke(command).IsSuccess());
    EXPECT_TRUE(mMockDelegate.mOnOff); // Should be ON
    EXPECT_TRUE(mMockTimerDelegate.IsTimerActive(&mCluster));

    // Verify Attributes
    uint16_t onTime = 0;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OnTime::Id, onTime), CHIP_NO_ERROR);
    EXPECT_EQ(onTime, 10);

    uint16_t offWaitTime = 0;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OffWaitTime::Id, offWaitTime), CHIP_NO_ERROR);
    EXPECT_EQ(offWaitTime, 20);

    // 2. Advance Clock (simulate 1 tick of 100ms)
    mMockTimerDelegate.AdvanceClock(System::Clock::Milliseconds32(100));

    // OnTime should decrement
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OnTime::Id, onTime), CHIP_NO_ERROR);
    EXPECT_EQ(onTime, 9);

    // 3. Exhaust OnTime
    for (int i = 0; i < 9; ++i)
    {
        mMockTimerDelegate.AdvanceClock(System::Clock::Milliseconds32(100));
    }

    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OnTime::Id, onTime), CHIP_NO_ERROR);
    EXPECT_EQ(onTime, 0);

    // Should now be OFF and OffWaitTime 0 (per spec/logic)
    // Wait, logic says: "If OnTime reaches 0, the server SHALL set the OffWaitTime and OnOff attributes to 0 and FALSE"
    EXPECT_FALSE(mMockDelegate.mOnOff);

    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OffWaitTime::Id, offWaitTime), CHIP_NO_ERROR);
    EXPECT_EQ(offWaitTime, 0);
}

TEST_F(TestOnOffLightingCluster, TestOffWithEffect)
{
    // 1. Turn On first
    EXPECT_TRUE(mClusterTester.Invoke(Commands::On::Type()).IsSuccess());
    EXPECT_TRUE(mMockDelegate.mOnOff);

    // 2. Off With Effect
    Commands::OffWithEffect::Type command;
    command.effectIdentifier = EffectIdentifierEnum::kDyingLight;
    command.effectVariant    = 10;

    EXPECT_TRUE(mClusterTester.Invoke(command).IsSuccess());

    // Check Effect Delegate
    EXPECT_TRUE(mMockEffectDelegate.mCalled);
    EXPECT_EQ(mMockEffectDelegate.mEffectId, EffectIdentifierEnum::kDyingLight);
    EXPECT_EQ(mMockEffectDelegate.mEffectVariant, 10);

    // Check State (Should be OFF)
    EXPECT_FALSE(mMockDelegate.mOnOff);

    // Check MarkSceneInvalid was called
    EXPECT_EQ(mMockScenesIntegrationDelegate.markInvalidCalls, 1);
}

TEST_F(TestOnOffLightingCluster, TestTimerCancellation)
{
    // 1. OnWithTimedOff (OnTime=10, OffWaitTime=0)
    Commands::OnWithTimedOff::Type command;
    command.onOffControl.SetField(OnOffControlBitmap::kAcceptOnlyWhenOn, 0);
    command.onTime      = 10;
    command.offWaitTime = 0;

    EXPECT_TRUE(mClusterTester.Invoke(command).IsSuccess());
    EXPECT_TRUE(mMockTimerDelegate.IsTimerActive(&mCluster));

    // 2. Send Off
    EXPECT_TRUE(mClusterTester.Invoke(Commands::Off::Type()).IsSuccess());
    EXPECT_FALSE(mMockDelegate.mOnOff);

    // Timer should be cancelled (because OffWaitTime is 0)
    EXPECT_FALSE(mMockTimerDelegate.IsTimerActive(&mCluster));
}

TEST_F(TestOnOffLightingCluster, TestOffWaitTime)
{
    // 1. OnWithTimedOff (OnTime=10, OffWaitTime=5)
    Commands::OnWithTimedOff::Type command;
    command.onOffControl.SetField(OnOffControlBitmap::kAcceptOnlyWhenOn, 0);
    command.onTime      = 10;
    command.offWaitTime = 5;

    EXPECT_TRUE(mClusterTester.Invoke(command).IsSuccess());

    // 2. Send Off (Manually)
    EXPECT_TRUE(mClusterTester.Invoke(Commands::Off::Type()).IsSuccess());
    EXPECT_FALSE(mMockDelegate.mOnOff);

    // Timer should be ACTIVE (for OffWaitTime)
    EXPECT_TRUE(mMockTimerDelegate.IsTimerActive(&mCluster));

    // 3. Advance clock 5 ticks
    for (int i = 0; i < 5; ++i)
    {
        mMockTimerDelegate.AdvanceClock(System::Clock::Milliseconds32(100));
    }

    // Timer should stop
    EXPECT_FALSE(mMockTimerDelegate.IsTimerActive(&mCluster));

    // OffWaitTime should be 0
    uint16_t offWaitTime = 1;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OffWaitTime::Id, offWaitTime), CHIP_NO_ERROR);
    EXPECT_EQ(offWaitTime, 0);
}

TEST_F(TestOnOffLightingCluster, TestGlobalSceneControl)
{
    // 1. Initial State: GlobalSceneControl = true
    bool globalSceneControl = false;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::GlobalSceneControl::Id, globalSceneControl), CHIP_NO_ERROR);
    EXPECT_TRUE(globalSceneControl);

    // 2. OffWithEffect -> Should set GlobalSceneControl = false
    Commands::OffWithEffect::Type offCommand;
    offCommand.effectIdentifier = EffectIdentifierEnum::kDyingLight;
    offCommand.effectVariant    = 0;
    EXPECT_TRUE(mClusterTester.Invoke(offCommand).IsSuccess());

    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::GlobalSceneControl::Id, globalSceneControl), CHIP_NO_ERROR);
    EXPECT_FALSE(globalSceneControl);

    // Verify StoreCurrentScene was called
    EXPECT_EQ(mMockScenesIntegrationDelegate.storeCalls.size(), 1u);
    // We don't have a great way to check the fabric index here, so we just check the call was made.

    // 3. OnWithRecallGlobalScene -> Should set GlobalSceneControl = true
    EXPECT_TRUE(mClusterTester.Invoke(Commands::OnWithRecallGlobalScene::Type()).IsSuccess());

    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::GlobalSceneControl::Id, globalSceneControl), CHIP_NO_ERROR);
    EXPECT_TRUE(globalSceneControl);

    // Verify RecallScene was called
    EXPECT_EQ(mMockScenesIntegrationDelegate.recallCalls.size(), 1u);
    // We don't have a great way to check the fabric index here, so we just check the call was made.
}

TEST_F(TestOnOffLightingCluster, TestSetOnOffWithTimeReset)
{
    // 1. Set OnTime and OffWaitTime to non-zero values
    EXPECT_EQ(mClusterTester.WriteAttribute(Attributes::OnTime::Id, static_cast<uint16_t>(100)), CHIP_NO_ERROR);
    EXPECT_EQ(mClusterTester.WriteAttribute(Attributes::OffWaitTime::Id, static_cast<uint16_t>(200)), CHIP_NO_ERROR);

    // 2. Call SetOnOffWithTimeReset(false)
    EXPECT_EQ(mCluster.SetOnOffWithTimeReset(false), CHIP_NO_ERROR);

    // Verify OnTime is reset
    uint16_t onTime = 1;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OnTime::Id, onTime), CHIP_NO_ERROR);
    EXPECT_EQ(onTime, 0);

    // OffWaitTime should not change
    uint16_t offWaitTime = 0;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OffWaitTime::Id, offWaitTime), CHIP_NO_ERROR);
    EXPECT_EQ(offWaitTime, 200);

    // 3. Set OnTime and OffWaitTime to non-zero values again
    EXPECT_EQ(mClusterTester.WriteAttribute(Attributes::OnTime::Id, static_cast<uint16_t>(100)), CHIP_NO_ERROR);
    EXPECT_EQ(mClusterTester.WriteAttribute(Attributes::OffWaitTime::Id, static_cast<uint16_t>(200)), CHIP_NO_ERROR);

    // 4. Call SetOnOffWithTimeReset(true)
    EXPECT_EQ(mCluster.SetOnOffWithTimeReset(true), CHIP_NO_ERROR);

    // Verify OffWaitTime is reset
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OffWaitTime::Id, offWaitTime), CHIP_NO_ERROR);
    EXPECT_EQ(offWaitTime, 0);

    // OnTime should not change
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OnTime::Id, onTime), CHIP_NO_ERROR);
    EXPECT_EQ(onTime, 100);
}

TEST_F(TestOnOffLightingCluster, TestOffWithEffect_GlobalSceneControlFalse)
{
    // 1. Turn On first
    EXPECT_TRUE(mClusterTester.Invoke(Commands::On::Type()).IsSuccess());
    EXPECT_TRUE(mMockDelegate.mOnOff);

    // 2. Set GlobalSceneControl to false (by invoking OffWithEffect once)
    Commands::OffWithEffect::Type offCommand1;
    offCommand1.effectIdentifier = EffectIdentifierEnum::kDyingLight;
    offCommand1.effectVariant    = 0;
    EXPECT_TRUE(mClusterTester.Invoke(offCommand1).IsSuccess());

    bool globalSceneControl = true;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::GlobalSceneControl::Id, globalSceneControl), CHIP_NO_ERROR);
    EXPECT_FALSE(globalSceneControl);
    EXPECT_EQ(mMockScenesIntegrationDelegate.storeCalls.size(), 1u); // Scene was stored

    // 3. Reset mock delegates
    mMockScenesIntegrationDelegate.storeCalls.clear();
    mMockEffectDelegate.mCalled = false;

    // 4. Turn On again
    EXPECT_TRUE(mClusterTester.Invoke(Commands::On::Type()).IsSuccess());
    EXPECT_TRUE(mMockDelegate.mOnOff);

    // 5. Call OffWithEffect again
    Commands::OffWithEffect::Type offCommand2;
    offCommand2.effectIdentifier = EffectIdentifierEnum::kDelayedAllOff;
    offCommand2.effectVariant    = 0;
    EXPECT_TRUE(mClusterTester.Invoke(offCommand2).IsSuccess());

    // Verify no scene operations are performed
    EXPECT_EQ(mMockScenesIntegrationDelegate.storeCalls.size(), 0u);

    // Verify effect delegate was called
    EXPECT_TRUE(mMockEffectDelegate.mCalled);
    EXPECT_EQ(mMockEffectDelegate.mEffectId, EffectIdentifierEnum::kDelayedAllOff);

    // Verify device is Off
    EXPECT_FALSE(mMockDelegate.mOnOff);
}

    // We don't have a great way to check the fabric index here, so we just check the call was made.
}

TEST_F(TestOnOffLightingCluster, TestOnWithRecallGlobalScene_GlobalSceneControlTrue)
{
    // Initial State: GlobalSceneControl = true (default)
    // OnOff is false initially as well.
    EXPECT_FALSE(mMockDelegate.mOnOff);

    // Invoke command
    EXPECT_TRUE(mClusterTester.Invoke(Commands::OnWithRecallGlobalScene::Type()).IsSuccess());

    // Verify recall was NOT called
    EXPECT_EQ(mMockScenesIntegrationDelegate.recallCalls.size(), 0u);

    // Verify OnOff state remains unchanged (still OFF)
    EXPECT_FALSE(mMockDelegate.mOnOff);

    // GlobalSceneControl should still be true
    bool globalSceneControl = false;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::GlobalSceneControl::Id, globalSceneControl), CHIP_NO_ERROR);
    EXPECT_TRUE(globalSceneControl);
}

// Custom mock delegate for simulating scene recall failure
class FailingRecallScenesIntegrationDelegate : public MockScenesIntegrationDelegate
{
public:
    CHIP_ERROR RecallGlobalScene(FabricIndex fabricIndex) override
    {
        recallCalls.push_back({ fabricIndex });
        return CHIP_ERROR_BAD_REQUEST; // Simulate failure
    }
};

TEST_F(TestOnOffLightingCluster, TestOnWithRecallGlobalScene_RecallFails)
{
    // 1. Set GlobalSceneControl to false (by invoking OffWithEffect once)
    Commands::OffWithEffect::Type offCommand;
    offCommand.effectIdentifier = EffectIdentifierEnum::kDyingLight;
    offCommand.effectVariant    = 0;
    EXPECT_TRUE(mClusterTester.Invoke(offCommand).IsSuccess());

    bool globalSceneControl = true;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::GlobalSceneControl::Id, globalSceneControl), CHIP_NO_ERROR);
    EXPECT_FALSE(globalSceneControl);
    EXPECT_EQ(mMockScenesIntegrationDelegate.storeCalls.size(), 1u); // Scene was stored

    mMockScenesIntegrationDelegate.recallCalls.clear(); // Clear calls from previous step

    // 2. Temporarily replace the scenes integration delegate with a failing one
    FailingRecallScenesIntegrationDelegate failingDelegate;
    mCluster.mScenesIntegrationDelegate = &failingDelegate;

    // Invoke OnWithRecallGlobalScene command
    EXPECT_TRUE(mClusterTester.Invoke(Commands::OnWithRecallGlobalScene::Type()).IsSuccess());

    // Verify recall was called once on the failing delegate
    EXPECT_EQ(failingDelegate.recallCalls.size(), 1u);

    // Verify OnOff state is true (fallback behavior)
    EXPECT_TRUE(mMockDelegate.mOnOff);

    // GlobalSceneControl should be true
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::GlobalSceneControl::Id, globalSceneControl), CHIP_NO_ERROR);
    EXPECT_TRUE(globalSceneControl);

    // Restore the original delegate
    mCluster.mScenesIntegrationDelegate = &mMockScenesIntegrationDelegate;
TEST_F(TestOnOffLightingCluster, TestOnWithTimedOff_AcceptOnlyWhenOn)
{
    // 1. Ensure device is OFF
    EXPECT_TRUE(mClusterTester.Invoke(Commands::Off::Type()).IsSuccess());
    EXPECT_FALSE(mMockDelegate.mOnOff);

    // 2. OnWithTimedOff with AcceptOnlyWhenOn = true
    Commands::OnWithTimedOff::Type command;
    command.onOffControl.SetField(OnOffControlBitmap::kAcceptOnlyWhenOn, 1);
    command.onTime      = 10;
    command.offWaitTime = 20;

    EXPECT_TRUE(mClusterTester.Invoke(command).IsSuccess());

    // Verify OnOff state remains false (command discarded)
    EXPECT_FALSE(mMockDelegate.mOnOff);

    // Verify timer is not active
    EXPECT_FALSE(mMockTimerDelegate.IsTimerActive(&mCluster));
}

TEST_F(TestOnOffLightingCluster, TestOnWithTimedOff_DelayedOffGuard)
{
    // 1. Ensure device is OFF
    EXPECT_TRUE(mClusterTester.Invoke(Commands::Off::Type()).IsSuccess());
    EXPECT_FALSE(mMockDelegate.mOnOff);

    // 2. Set OffWaitTime to a non-zero value
    EXPECT_EQ(mClusterTester.WriteAttribute(Attributes::OffWaitTime::Id, static_cast<uint16_t>(5)), CHIP_NO_ERROR);
    uint16_t offWaitTime = 0;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OffWaitTime::Id, offWaitTime), CHIP_NO_ERROR);
    EXPECT_EQ(offWaitTime, 5);

    // 3. Call OnWithTimedOff (should be ignored due to OffWaitTime > 0 and OnOff is false)
    Commands::OnWithTimedOff::Type command;
    command.onOffControl.SetField(OnOffControlBitmap::kAcceptOnlyWhenOn, 0); // Unconditional
    command.onTime      = 10;
    command.offWaitTime = 20;

    EXPECT_TRUE(mClusterTester.Invoke(command).IsSuccess());

    // Verify OnOff state remains false
    EXPECT_FALSE(mMockDelegate.mOnOff);

    // Verify OffWaitTime is reduced to min(current, new)
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::OffWaitTime::Id, offWaitTime), CHIP_NO_ERROR);
    EXPECT_EQ(offWaitTime, std::min(static_cast<uint16_t>(5), static_cast<uint16_t>(20)));

    // Verify timer is active (decrementing OffWaitTime)
    EXPECT_TRUE(mMockTimerDelegate.IsTimerActive(&mCluster));
}

struct TestOnOffLightingCluster_Startup : public ::testing::Test
{
    static void SetUpTestSuite() { ASSERT_EQ(Platform::MemoryInit(), CHIP_NO_ERROR); }
    static void TearDownTestSuite() { Platform::MemoryShutdown(); }

    void SetUp() override
    {
        VerifyOrDie(mPersistenceProvider.Init(&mContext.Get().storage) == CHIP_NO_ERROR);
        app::SetSafeAttributePersistenceProvider(&mPersistenceProvider);
    }

    void TearDown() override { app::SetSafeAttributePersistenceProvider(nullptr); }

    template <typename T>
    void WriteAttribute(AttributeId id, const T & value)
    {
        AttributePersistence provider(mContext.Get().attributeStorage);
        EXPECT_EQ(provider.WriteValue(ConcreteAttributePath(kTestEndpointId, OnOff::Id, id),
                                      ByteSpan(from_const_char(reinterpret_cast<const char *>(&value)), sizeof(value))),
                  CHIP_NO_ERROR);
    }

    TestServerClusterContext mContext;
    app::DefaultSafeAttributePersistenceProvider mPersistenceProvider;
};

TEST_F(TestOnOffLightingCluster_Startup, TestStartupOnOff_On)
{
    // Pre-Requisite: Persisted OnOff is false
    WriteAttribute(Attributes::OnOff::Id, false);
    WriteAttribute(Attributes::StartUpOnOff::Id, StartUpOnOffEnum::kOn);

    MockOnOffDelegate mockDelegate;
    TimerDelegateMock mockTimerDelegate;
    MockOnOffEffectDelegate mockEffectDelegate;
    MockScenesIntegrationDelegate mockScenesIntegrationDelegate;
    OnOffLightingCluster cluster(kTestEndpointId, mockTimerDelegate, mockEffectDelegate, &mockScenesIntegrationDelegate);

    cluster.AddDelegate(&mockDelegate);
    EXPECT_EQ(cluster.Startup(mContext.Get()), CHIP_NO_ERROR);

    EXPECT_TRUE(mockDelegate.mStartupCalled);
    EXPECT_TRUE(mockDelegate.mOnOff);

    ClusterTester tester(cluster);
    bool onOff = false;
    EXPECT_EQ(tester.ReadAttribute(Attributes::OnOff::Id, onOff), CHIP_NO_ERROR);
    EXPECT_TRUE(onOff);
}

TEST_F(TestOnOffLightingCluster_Startup, TestStartupOnOff_Off)
{
    // Pre-Requisite: Persisted OnOff is true
    WriteAttribute(Attributes::OnOff::Id, true);
    WriteAttribute(Attributes::StartUpOnOff::Id, StartUpOnOffEnum::kOff);

    MockOnOffDelegate mockDelegate;
    TimerDelegateMock mockTimerDelegate;
    MockOnOffEffectDelegate mockEffectDelegate;
    MockScenesIntegrationDelegate mockScenesIntegrationDelegate;
    OnOffLightingCluster cluster(kTestEndpointId, mockTimerDelegate, mockEffectDelegate, &mockScenesIntegrationDelegate);

    cluster.AddDelegate(&mockDelegate);
    EXPECT_EQ(cluster.Startup(mContext.Get()), CHIP_NO_ERROR);

    EXPECT_TRUE(mockDelegate.mStartupCalled);
    EXPECT_FALSE(mockDelegate.mOnOff);

    ClusterTester tester(cluster);
    bool onOff = true;
    EXPECT_EQ(tester.ReadAttribute(Attributes::OnOff::Id, onOff), CHIP_NO_ERROR);
    EXPECT_FALSE(onOff);
}

TEST_F(TestOnOffLightingCluster_Startup, TestStartupOnOff_Toggle)
{
    // 1. From OFF to ON
    {
        WriteAttribute(Attributes::OnOff::Id, false);
        WriteAttribute(Attributes::StartUpOnOff::Id, StartUpOnOffEnum::kToggle);

        MockOnOffDelegate mockDelegate;
        TimerDelegateMock mockTimerDelegate;
        MockOnOffEffectDelegate mockEffectDelegate;
        MockScenesIntegrationDelegate mockScenesIntegrationDelegate;
        OnOffLightingCluster cluster(kTestEndpointId, mockTimerDelegate, mockEffectDelegate, &mockScenesIntegrationDelegate);

        cluster.AddDelegate(&mockDelegate);
        EXPECT_EQ(cluster.Startup(mContext.Get()), CHIP_NO_ERROR);
        EXPECT_TRUE(mockDelegate.mOnOff);
    }

    // 2. From ON to OFF
    {
        WriteAttribute(Attributes::OnOff::Id, true);
        WriteAttribute(Attributes::StartUpOnOff::Id, StartUpOnOffEnum::kToggle);

        MockOnOffDelegate mockDelegate;
        TimerDelegateMock mockTimerDelegate;
        MockOnOffEffectDelegate mockEffectDelegate;
        MockScenesIntegrationDelegate mockScenesIntegrationDelegate;
        OnOffLightingCluster cluster(kTestEndpointId, mockTimerDelegate, mockEffectDelegate, &mockScenesIntegrationDelegate);

        cluster.AddDelegate(&mockDelegate);
        EXPECT_EQ(cluster.Startup(mContext.Get()), CHIP_NO_ERROR);
        EXPECT_FALSE(mockDelegate.mOnOff);
    }
}

TEST_F(TestOnOffLightingCluster_Startup, TestStartupOnOff_Null)
{
    // Persisted OnOff is true, StartUpOnOff is null (not written)
    WriteAttribute(Attributes::OnOff::Id, true);

    MockOnOffDelegate mockDelegate;
    TimerDelegateMock mockTimerDelegate;
    MockOnOffEffectDelegate mockEffectDelegate;
    MockScenesIntegrationDelegate mockScenesIntegrationDelegate;
    OnOffLightingCluster cluster(kTestEndpointId, mockTimerDelegate, mockEffectDelegate, &mockScenesIntegrationDelegate);

    cluster.AddDelegate(&mockDelegate);
    EXPECT_EQ(cluster.Startup(mContext.Get()), CHIP_NO_ERROR);

    // Should remain ON
    EXPECT_TRUE(mockDelegate.mOnOff);
}

TEST_F(TestOnOffLightingCluster, TestWriteOnTimeUpdatesTimer)
{
    // Initially timer is not active
    EXPECT_FALSE(mMockTimerDelegate.IsTimerActive(&mCluster));

    // 1. Write non-zero OnTime
    EXPECT_EQ(mClusterTester.WriteAttribute(Attributes::OnTime::Id, static_cast<uint16_t>(100)), CHIP_NO_ERROR);

    // Verify timer is active
    EXPECT_TRUE(mMockTimerDelegate.IsTimerActive(&mCluster));

    // 2. Write 0 to OnTime
    EXPECT_EQ(mClusterTester.WriteAttribute(Attributes::OnTime::Id, static_cast<uint16_t>(0)), CHIP_NO_ERROR);

    // Verify timer is cancelled
    EXPECT_FALSE(mMockTimerDelegate.IsTimerActive(&mCluster));
}

TEST_F(TestOnOffLightingCluster, TestWriteOffWaitTimeUpdatesTimer)
{
    // 1. Ensure device is OFF
    EXPECT_TRUE(mClusterTester.Invoke(Commands::Off::Type()).IsSuccess());
    EXPECT_FALSE(mMockDelegate.mOnOff);
    EXPECT_FALSE(mMockTimerDelegate.IsTimerActive(&mCluster));

    // 2. Write non-zero OffWaitTime
    EXPECT_EQ(mClusterTester.WriteAttribute(Attributes::OffWaitTime::Id, static_cast<uint16_t>(100)), CHIP_NO_ERROR);

    // Verify timer is active
    EXPECT_TRUE(mMockTimerDelegate.IsTimerActive(&mCluster));

    // 3. Write 0 to OffWaitTime
    EXPECT_EQ(mClusterTester.WriteAttribute(Attributes::OffWaitTime::Id, static_cast<uint16_t>(0)), CHIP_NO_ERROR);

    // Verify timer is cancelled
    EXPECT_FALSE(mMockTimerDelegate.IsTimerActive(&mCluster));
}

TEST_F(TestOnOffLightingCluster, TestWriteOffWaitTimeUpdatesTimer)
{
    // 1. Ensure device is OFF
    EXPECT_TRUE(mClusterTester.Invoke(Commands::Off::Type()).IsSuccess());
    EXPECT_FALSE(mMockDelegate.mOnOff);
    EXPECT_FALSE(mMockTimerDelegate.IsTimerActive(&mCluster));

    // 2. Write non-zero OffWaitTime
    EXPECT_EQ(mClusterTester.WriteAttribute(Attributes::OffWaitTime::Id, static_cast<uint16_t>(100)), CHIP_NO_ERROR);

    // Verify timer is active
    EXPECT_TRUE(mMockTimerDelegate.IsTimerActive(&mCluster));

    // 3. Write 0 to OffWaitTime
    EXPECT_EQ(mClusterTester.WriteAttribute(Attributes::OffWaitTime::Id, static_cast<uint16_t>(0)), CHIP_NO_ERROR);

    // Verify timer is cancelled
    EXPECT_FALSE(mMockTimerDelegate.IsTimerActive(&mCluster));
}

// Test fixture for OnOffLightingCluster with DeadFrontBehavior
struct TestDeadFrontOnOffCluster : public ::testing::Test
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
    TimerDelegateMock mMockTimerDelegate;
    MockOnOffEffectDelegate mMockEffectDelegate;
    MockScenesIntegrationDelegate mMockScenesIntegrationDelegate;

    OnOffLightingCluster mCluster{ kTestEndpointId, mMockTimerDelegate, mMockEffectDelegate, &mMockScenesIntegrationDelegate, BitMask<Feature>(Feature::kDeadFrontBehavior) };

    ClusterTester mClusterTester{ mCluster };

    app::DefaultSafeAttributePersistenceProvider mPersistenceProvider;
};

TEST_F(TestDeadFrontOnOffCluster, TestFeatureMap)
{
    uint32_t featureMap = 0;
    EXPECT_EQ(mClusterTester.ReadAttribute(Attributes::FeatureMap::Id, featureMap), CHIP_NO_ERROR);
    EXPECT_EQ(featureMap, static_cast<uint32_t>(Feature::kDeadFrontBehavior));
}

} // namespace
