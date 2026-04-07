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
#include <pw_unit_test/framework.h>

#include <app/data-model-provider/AttributeChangeListener.h>
#include <app/data-model-provider/Provider.h>
#include <lib/core/CHIPError.h>
#include <protocols/interaction_model/StatusCode.h>

namespace {

using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;

using chip::Protocols::InteractionModel::Status;

// Minimal Concrete Provider for testing
class TestProvider : public Provider
{
public:
    // Implement pure virtuals from ProviderMetadataTree
    CHIP_ERROR Endpoints(ReadOnlyBufferBuilder<EndpointEntry> & builder) override { return CHIP_NO_ERROR; }
    CHIP_ERROR DeviceTypes(EndpointId endpointId, ReadOnlyBufferBuilder<DeviceTypeEntry> & builder) override
    {
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR ClientClusters(EndpointId endpointId, ReadOnlyBufferBuilder<ClusterId> & builder) override { return CHIP_NO_ERROR; }
    CHIP_ERROR ServerClusters(EndpointId endpointId, ReadOnlyBufferBuilder<ServerClusterEntry> & builder) override
    {
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR EventInfo(const ConcreteEventPath & path, EventEntry & eventInfo) override { return CHIP_NO_ERROR; }
    CHIP_ERROR Attributes(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<AttributeEntry> & builder) override
    {
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR GeneratedCommands(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<CommandId> & builder) override
    {
        return CHIP_NO_ERROR;
    }
    CHIP_ERROR AcceptedCommands(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<AcceptedCommandEntry> & builder) override
    {
        return CHIP_NO_ERROR;
    }

    // Implement pure virtuals from Provider
    ActionReturnStatus ReadAttribute(const ReadAttributeRequest & request, AttributeValueEncoder & encoder) override
    {
        return Status::Success;
    }
    ActionReturnStatus WriteAttribute(const WriteAttributeRequest & request, AttributeValueDecoder & decoder) override
    {
        return Status::Success;
    }
    void ListAttributeWriteNotification(const ConcreteAttributePath & aPath, ListWriteOperation opType,
                                        FabricIndex accessingFabric) override
    {}
    std::optional<ActionReturnStatus> InvokeCommand(const InvokeRequest & request, chip::TLV::TLVReader & input_arguments,
                                                    CommandHandler * handler) override
    {
        return Status::Success;
    }
};

// Mock Listener
class TestListener : public AttributeChangeListener
{
public:
    int callCount = 0;
    ConcreteAttributePath lastPath;
    AttributeChangeType lastType;

    void OnAttributeChanged(const ConcreteAttributePath & path, AttributeChangeType type) override
    {
        callCount++;
        lastPath = path;
        lastType = type;
    }
};

TEST(TestProviderListener, TestSingleListener)
{
    TestProvider provider;
    TestListener listener;

    provider.RegisterAttributeChangeListener(listener);

    ConcreteAttributePath path(1, 1, 1);
    provider.NotifyAttributeChanged(path, AttributeChangeType::kReportable);

    EXPECT_EQ(listener.callCount, 1);
    EXPECT_EQ(listener.lastPath.mEndpointId, 1u);
    EXPECT_EQ(listener.lastPath.mClusterId, 1u);
    EXPECT_EQ(listener.lastPath.mAttributeId, 1u);
    EXPECT_EQ(listener.lastType, AttributeChangeType::kReportable);

    provider.UnregisterAttributeChangeListener(listener);
    provider.NotifyAttributeChanged(path, AttributeChangeType::kQuiet);
    EXPECT_EQ(listener.callCount, 1); // Should not have incremented
}

TEST(TestProviderListener, TestMultipleListeners)
{
    TestProvider provider;
    TestListener listener1;
    TestListener listener2;

    provider.RegisterAttributeChangeListener(listener1);
    provider.RegisterAttributeChangeListener(listener2);

    ConcreteAttributePath path(1, 2, 3);
    provider.NotifyAttributeChanged(path, AttributeChangeType::kQuiet);

    EXPECT_EQ(listener1.callCount, 1);
    EXPECT_EQ(listener1.lastPath.mEndpointId, 1u);
    EXPECT_EQ(listener1.lastType, AttributeChangeType::kQuiet);

    EXPECT_EQ(listener2.callCount, 1);
    EXPECT_EQ(listener2.lastPath.mEndpointId, 1u);
    EXPECT_EQ(listener2.lastType, AttributeChangeType::kQuiet);

    provider.UnregisterAttributeChangeListener(listener1);
    provider.NotifyAttributeChanged(path, AttributeChangeType::kReportable);

    EXPECT_EQ(listener1.callCount, 1); // Unregistered
    EXPECT_EQ(listener2.callCount, 2); // Still registered
    EXPECT_EQ(listener2.lastType, AttributeChangeType::kReportable);
}

} // namespace
