/*
 *
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

#include <access/Privilege.h>
#include <app-common/zap-generated/cluster-enums.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app/AttributeValueEncoder.h>
#include <app/codegen-data-model-provider/tests/AttributeReportIBEncodeDecode.h>
#include <app/data-model-provider/Context.h>
#include <app/data-model-provider/OperationTypes.h>
#include <app/data-model-provider/StringBuilderAdapters.h>
#include <app/dynamic-data-model-provider/ClusterProvider.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/StringBuilderAdapters.h>
#include <lib/core/TLVReader.h>
#include <lib/support/BitFlags.h>
#include <protocols/interaction_model/StatusCode.h>

#include <app-common/zap-generated/cluster-objects.h>

#include <pw_unit_test/framework.h>

namespace {

using namespace chip::app;
using namespace chip;
using namespace chip::app::DynamicDataModel;

constexpr FabricIndex kTestFabrixIndex = kMinValidFabricIndex;
constexpr NodeId kTestNodeId           = 0xFFFF'1234'ABCD'4321;

constexpr Access::SubjectDescriptor kAdminSubjectDescriptor{
    .fabricIndex = kTestFabrixIndex,
    .authMode    = Access::AuthMode::kCase,
    .subject     = kTestNodeId,
};

class TestCluster : public Cluster<2 /* kAttributeCount */>
{
public:
    using Base = Cluster<2>;

    constexpr TestCluster() :
        Base({
            AttributeDefinition(Clusters::UnitTesting::Attributes::Int24u::Id) //
                .SetReadFunction(ReadVia(this, &TestCluster::GetInt24Value)),
            AttributeDefinition(Clusters::UnitTesting::Attributes::Bitmap8::Id) //
                .SetReadFunction(ReadVia(this, &TestCluster::GetBitmap8Value))
                .SetWriteFunction(WriteVia(this, &TestCluster::SetBitmap8Value))
                .SetWritePrivilege(chip::Access::Privilege::kAdminister),
        })
    {}

    uint32_t GetInt24Value() { return mInt24Value; }

    chip::BitMask<Clusters::UnitTesting::Bitmap8MaskMap> GetBitmap8Value() { return mMaskValue; }

    CHIP_ERROR SetBitmap8Value(const chip::BitMask<Clusters::UnitTesting::Bitmap8MaskMap> & value)
    {
        mMaskValue = value;
        return CHIP_NO_ERROR;
    }

    void TestSetInt24Value(uint32_t v) { mInt24Value = v; }

private:
    uint32_t mInt24Value = 123;
    chip::BitMask<Clusters::UnitTesting::Bitmap8MaskMap> mMaskValue;
};

/// Contains a `ReadAttributeRequest` as well as classes to convert this into a AttributeReportIBs
/// and later decode it
///
/// It wraps boilerplate code to obtain a `AttributeValueEncoder` as well as later decoding
/// the underlying encoded data for verification.
struct TestReadRequest
{
    DataModel::ReadAttributeRequest request;

    // encoded-used classes
    chip::Test::EncodedReportIBs encodedIBs;
    AttributeReportIBs::Builder reportBuilder;
    std::unique_ptr<AttributeValueEncoder> encoder;

    TestReadRequest(const chip::Access::SubjectDescriptor & subject, const ConcreteAttributePath & path)
    {
        // operationFlags is 0 i.e. not internal
        // readFlags is 0 i.e. not fabric filtered
        // dataVersion is missing (no data version filtering)
        request.subjectDescriptor = subject;
        request.path              = path;
    }

    std::unique_ptr<AttributeValueEncoder> StartEncoding(chip::DataVersion dataVersion,
                                                         AttributeEncodeState state = AttributeEncodeState())
    {
        CHIP_ERROR err = encodedIBs.StartEncoding(reportBuilder);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(Test, "FAILURE starting encoding %" CHIP_ERROR_FORMAT, err.Format());
            return nullptr;
        }

        // TODO: could we test isFabricFiltered and EncodeState?

        // request.subjectDescriptor is known non-null because it is set in the constructor
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        return std::make_unique<AttributeValueEncoder>(reportBuilder, *request.subjectDescriptor, request.path, dataVersion,
                                                       false /* aIsFabricFiltered */, state);
    }

    CHIP_ERROR FinishEncoding() { return encodedIBs.FinishEncoding(reportBuilder); }
};

} // namespace

TEST(TestClusterProvider, BasicRead)
{
    TestCluster testClusters;

    // Minimal data required
    DataModel::InteractionModelContext context{ nullptr, nullptr, nullptr };

    {
        TestReadRequest read_request(
            kAdminSubjectDescriptor,
            ConcreteAttributePath(0 /* kEndpointId */, 0 /* kClusterId */, Clusters::UnitTesting::Attributes::Boolean::Id));

        std::unique_ptr<AttributeValueEncoder> encoder = read_request.StartEncoding(123 /* dataVersion */);
        ASSERT_TRUE(encoder);

        // attempt to read an unsupported attribute should error out
        ASSERT_EQ(testClusters.ReadAttribute(context, read_request.request, *encoder.get()),
                  Protocols::InteractionModel::Status::UnsupportedRead);
    }

    constexpr uint32_t kUint32TestValues[] = { 0x1234, 0, 1234, 4321, 100, 0xFFFFFF, 18 };

    for (uint32_t testValue : kUint32TestValues)
    {
        testClusters.TestSetInt24Value(testValue);

        constexpr chip::DataVersion kTestDataVersion = 112233;

        TestReadRequest read_request(
            kAdminSubjectDescriptor,
            ConcreteAttributePath(0 /* kEndpointId */, 0 /* kClusterId */, Clusters::UnitTesting::Attributes::Int24u::Id));

        std::unique_ptr<AttributeValueEncoder> encoder = read_request.StartEncoding(kTestDataVersion);
        ASSERT_TRUE(encoder);

        // attempt to read
        ASSERT_EQ(testClusters.ReadAttribute(context, read_request.request, *encoder.get()), CHIP_NO_ERROR);
        ASSERT_EQ(read_request.FinishEncoding(), CHIP_NO_ERROR);

        std::vector<chip::Test::DecodedAttributeData> items;
        ASSERT_EQ(read_request.encodedIBs.Decode(items), CHIP_NO_ERROR);

        ASSERT_EQ(items.size(), 1u);

        const chip::Test::DecodedAttributeData & data = items[0];
        ASSERT_EQ(data.dataVersion, kTestDataVersion);

        chip::TLV::TLVReader reader(data.dataReader);
        uint32_t readValue = 0;
        ASSERT_EQ(reader.Get(readValue), CHIP_NO_ERROR);
        ASSERT_EQ(readValue, testValue);
    }

    // Test bitmap8 values
    constexpr chip::BitMask<Clusters::UnitTesting::Bitmap8MaskMap> kBitmapTestValues[] = {
        chip::BitFlags<Clusters::UnitTesting::Bitmap8MaskMap>(Clusters::UnitTesting::Bitmap8MaskMap::kMaskVal1),
        chip::BitFlags<Clusters::UnitTesting::Bitmap8MaskMap>(0),
        chip::BitFlags<Clusters::UnitTesting::Bitmap8MaskMap>(Clusters::UnitTesting::Bitmap8MaskMap::kMaskVal1,
                                                              Clusters::UnitTesting::Bitmap8MaskMap::kMaskVal2,
                                                              Clusters::UnitTesting::Bitmap8MaskMap::kMaskVal4),
        chip::BitFlags<Clusters::UnitTesting::Bitmap8MaskMap>(Clusters::UnitTesting::Bitmap8MaskMap::kMaskVal3,
                                                              Clusters::UnitTesting::Bitmap8MaskMap::kMaskVal4),

    };
    for (auto testValue : kBitmapTestValues)
    {
        testClusters.SetBitmap8Value(testValue);

        constexpr chip::DataVersion kTestDataVersion = 112233;

        TestReadRequest read_request(
            kAdminSubjectDescriptor,
            ConcreteAttributePath(0 /* kEndpointId */, 0 /* kClusterId */, Clusters::UnitTesting::Attributes::Bitmap8::Id));

        std::unique_ptr<AttributeValueEncoder> encoder = read_request.StartEncoding(kTestDataVersion);
        ASSERT_TRUE(encoder);

        // attempt to read
        ASSERT_EQ(testClusters.ReadAttribute(context, read_request.request, *encoder.get()), CHIP_NO_ERROR);
        ASSERT_EQ(read_request.FinishEncoding(), CHIP_NO_ERROR);

        std::vector<chip::Test::DecodedAttributeData> items;
        ASSERT_EQ(read_request.encodedIBs.Decode(items), CHIP_NO_ERROR);

        ASSERT_EQ(items.size(), 1u);

        const chip::Test::DecodedAttributeData & data = items[0];
        ASSERT_EQ(data.dataVersion, kTestDataVersion);

        chip::TLV::TLVReader reader(data.dataReader);
        BitMask<Clusters::UnitTesting::Bitmap8MaskMap> readValue = 0;
        ASSERT_EQ(reader.Get(readValue), CHIP_NO_ERROR);
        ASSERT_EQ(readValue, testValue);
    }
}

TEST(TestClusterProvider, BasicWrite)
{
    // TODO: implement a basic write test, including the ability to have encoders ...
    TestCluster testClusters;

    // Minimal data required
    DataModel::InteractionModelContext context{ nullptr, nullptr, nullptr };

    {
        DataModel::WriteAttributeRequest request;

        request.subjectDescriptor = kAdminSubjectDescriptor;
        request.path =
            ConcreteAttributePath(0 /* kEndpointId */, 0 /* kClusterId */, Clusters::UnitTesting::Attributes::Int24u::Id);

        TLV::TLVReader reader; // FIXME: how do I fill this one up?

        AttributeValueDecoder decoder(reader, kAdminSubjectDescriptor);

        // attempt to read an unsupported attribute should error out
        ASSERT_EQ(testClusters.WriteAttribute(context, request, decoder), Protocols::InteractionModel::Status::UnsupportedWrite);
    }
}
