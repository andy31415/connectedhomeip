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

#include <data-model-providers/codegen/EmberAttributeDecoder.h>
#include <app-common/zap-generated/attribute-type.h>
#include <app/data-model/Encode.h>
#include <lib/support/tests/ExtraPwTestMacros.h>

namespace chip {
namespace app {
namespace {

class MockServerCluster : public ServerClusterInterface
{
public:
    MockServerCluster(EndpointId endpoint, ClusterId cluster) : mPath(endpoint, cluster) {}

    CHIP_ERROR Startup(ServerClusterContext & context) override { return CHIP_NO_ERROR; }
    void Shutdown(ClusterShutdownType shutdownType) override {}

    Span<const ConcreteClusterPath> GetPaths() const override { return Span<const ConcreteClusterPath>(&mPath, 1); }

    DataVersion GetDataVersion(const ConcreteClusterPath & path) const override { return 0; }
    BitFlags<DataModel::ClusterQualityFlags> GetClusterFlags(const ConcreteClusterPath &) const override { return {}; }

    DataModel::ActionReturnStatus ReadAttribute(const DataModel::ReadAttributeRequest & request,
                                                AttributeValueEncoder & encoder) override
    {
        if (request.path.mAttributeId == mAttributeId)
        {
            return encoder.Encode(mBoolValue);
        }
        return Protocols::InteractionModel::Status::UnsupportedAttribute;
    }

    DataModel::ActionReturnStatus WriteAttribute(const DataModel::WriteAttributeRequest & request,
                                                 AttributeValueDecoder & decoder) override
    {
        return Protocols::InteractionModel::Status::UnsupportedWrite;
    }

    CHIP_ERROR Attributes(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder) override
    {
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR EventInfo(const ConcreteEventPath & path, DataModel::EventEntry & eventInfo) override
    {
        return CHIP_ERROR_NOT_IMPLEMENTED;
    }

    std::optional<DataModel::ActionReturnStatus>
    InvokeCommand(const DataModel::InvokeRequest & request, chip::TLV::TLVReader & input_arguments, CommandHandler * handler) override
    {
        return std::nullopt;
    }

    CHIP_ERROR AcceptedCommands(const ConcreteClusterPath & path,
                                ReadOnlyBufferBuilder<DataModel::AcceptedCommandEntry> & builder) override
    {
        return CHIP_NO_ERROR;
    }

    CHIP_ERROR GeneratedCommands(const ConcreteClusterPath & path, ReadOnlyBufferBuilder<CommandId> & builder) override
    {
        return CHIP_NO_ERROR;
    }

    void SetBoolValue(bool value) { mBoolValue = value; }
    void SetAttributeId(AttributeId id) { mAttributeId = id; }

private:
    ConcreteClusterPath mPath;
    AttributeId mAttributeId = 1;
    bool mBoolValue = false;
};

TEST(TestEmberAttributeDecoder, TestDecodeBool)
{
    MockServerCluster cluster(1, 2);
    cluster.SetAttributeId(3);
    cluster.SetBoolValue(true);

    ConcreteAttributePath path(1, 2, 3);
    AttributeDecoderParams params{
        .path = path,
        .cluster = cluster,
        .emberType = ZCL_BOOLEAN_ATTRIBUTE_TYPE,
        .emberSize = 1
    };

    uint8_t buffer[10];
    MutableByteSpan outBuffer(buffer);

    CHIP_ERROR err = DecodeAttributeToEmberBuffer(params, outBuffer);
    EXPECT_EQ(err, CHIP_NO_ERROR);
    EXPECT_EQ(outBuffer.size(), 1u);
    EXPECT_EQ(buffer[0], 1);

    cluster.SetBoolValue(false);
    outBuffer = MutableByteSpan(buffer);
    err = DecodeAttributeToEmberBuffer(params, outBuffer);
    EXPECT_EQ(err, CHIP_NO_ERROR);
    EXPECT_EQ(outBuffer.size(), 1u);
    EXPECT_EQ(buffer[0], 0);
}

} // namespace
} // namespace app
} // namespace chip
