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

    OnOffLightingCluster mCluster{ kTestEndpointId, mMockTimerDelegate, mMockEffectDelegate };

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
}

} // namespace
