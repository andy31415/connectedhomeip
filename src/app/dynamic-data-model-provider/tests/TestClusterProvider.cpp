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

#include <access/Privilege.h>
#include <app-common/zap-generated/cluster-enums.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app/AttributeValueEncoder.h>
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

#include <app/data-model-provider/tests/ReadTesting.h>
#include <app/data-model-provider/tests/TestConstants.h>

#include <app-common/zap-generated/cluster-objects.h>

#include <pw_unit_test/framework.h>

namespace {

using namespace chip;
using namespace chip::app;
using namespace chip::app::DynamicDataModel;
using namespace chip::app::Testing;

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

// Sets up data for writing
struct TestWriteRequest
{
    DataModel::WriteAttributeRequest request;
    uint8_t tlvBuffer[128] = { 0 };
    TLV::TLVReader
        tlvReader; /// tlv reader used for the returned AttributeValueDecoder (since attributeValueDecoder uses references)

    TestWriteRequest(const Access::SubjectDescriptor & subject, const ConcreteDataAttributePath & path)
    {
        request.subjectDescriptor = subject;
        request.path              = path;
    }

    template <typename T>
    TLV::TLVReader ReadEncodedValue(const T & value)
    {
        TLV::TLVWriter writer;
        writer.Init(tlvBuffer);

        // Encoding is within a structure:
        //   - BEGIN_STRUCT
        //     - 1: .....
        //   - END_STRUCT
        TLV::TLVType outerContainerType;
        VerifyOrDie(writer.StartContainer(TLV::AnonymousTag(), TLV::kTLVType_Structure, outerContainerType) == CHIP_NO_ERROR);
        VerifyOrDie(chip::app::DataModel::Encode(writer, TLV::ContextTag(1), value) == CHIP_NO_ERROR);
        VerifyOrDie(writer.EndContainer(outerContainerType) == CHIP_NO_ERROR);
        VerifyOrDie(writer.Finalize() == CHIP_NO_ERROR);

        TLV::TLVReader reader;
        reader.Init(tlvBuffer);

        // position the reader inside the buffer, on the encoded value
        VerifyOrDie(reader.Next() == CHIP_NO_ERROR);
        VerifyOrDie(reader.EnterContainer(outerContainerType) == CHIP_NO_ERROR);
        VerifyOrDie(reader.Next() == CHIP_NO_ERROR);

        return reader;
    }

    template <class T>
    AttributeValueDecoder DecoderFor(const T & value)
    {
        tlvReader = ReadEncodedValue(value);
        return AttributeValueDecoder(tlvReader, request.subjectDescriptor.value_or(kDenySubjectDescriptor));
    }
};

} // namespace

TEST(TestClusterProvider, BasicRead)
{
    TestCluster testClusters;

    // Minimal data required
    DataModel::InteractionModelContext context{ nullptr, nullptr, nullptr };

    {
        TestReadRequest read_request(TestReadRequest::ConstructionArguments(
            ConcreteAttributePath(0 /* kEndpointId */, 0 /* kClusterId */, Clusters::UnitTesting::Attributes::Boolean::Id)));

        std::unique_ptr<AttributeValueEncoder> encoder = read_request.StartEncoding();
        ASSERT_TRUE(encoder);

        // attempt to read an unsupported attribute should error out
        ASSERT_EQ(testClusters.ReadAttribute(context, read_request.GetRequest(), *encoder.get()),
                  Protocols::InteractionModel::Status::UnsupportedRead);
    }

    constexpr uint32_t kUint32TestValues[] = { 0x1234, 0, 1234, 4321, 100, 0xFFFFFF, 18 };

    for (uint32_t testValue : kUint32TestValues)
    {
        testClusters.TestSetInt24Value(testValue);

        constexpr chip::DataVersion kTestDataVersion = 112233;

        TestReadRequest read_request(TestReadRequest::ConstructionArguments(
            ConcreteAttributePath(0 /* kEndpointId */, 0 /* kClusterId */, Clusters::UnitTesting::Attributes::Int24u::Id)));

        std::unique_ptr<AttributeValueEncoder> encoder =
            read_request.StartEncoding(TestReadRequest::EncodingParams().SetDataVersion(kTestDataVersion));
        ASSERT_TRUE(encoder);

        // attempt to read
        ASSERT_EQ(testClusters.ReadAttribute(context, read_request.GetRequest(), *encoder.get()), CHIP_NO_ERROR);
        ASSERT_EQ(read_request.FinishEncoding(), CHIP_NO_ERROR);

        std::vector<DecodedAttributeData> items;
        ASSERT_EQ(read_request.GetEncodedIBs().Decode(items), CHIP_NO_ERROR);

        ASSERT_EQ(items.size(), 1u);

        const DecodedAttributeData & data = items[0];
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

        TestReadRequest read_request(TestReadRequest::ConstructionArguments(
            ConcreteAttributePath(0 /* kEndpointId */, 0 /* kClusterId */, Clusters::UnitTesting::Attributes::Bitmap8::Id)));

        std::unique_ptr<AttributeValueEncoder> encoder =
            read_request.StartEncoding(TestReadRequest::EncodingParams().SetDataVersion(kTestDataVersion));
        ASSERT_TRUE(encoder);

        // attempt to read
        ASSERT_EQ(testClusters.ReadAttribute(context, read_request.GetRequest(), *encoder.get()), CHIP_NO_ERROR);
        ASSERT_EQ(read_request.FinishEncoding(), CHIP_NO_ERROR);

        std::vector<DecodedAttributeData> items;
        ASSERT_EQ(read_request.GetEncodedIBs().Decode(items), CHIP_NO_ERROR);

        ASSERT_EQ(items.size(), 1u);

        const DecodedAttributeData & data = items[0];
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
        TestWriteRequest request(
            kAdminSubjectDescriptor,
            ConcreteAttributePath(0 /* kEndpointId */, 0 /* kClusterId */, Clusters::UnitTesting::Attributes::Bitmap8::Id));

        AttributeValueDecoder decoder = request.DecoderFor(testValue);

        // Writing of the values should succeed and the written value should match
        ASSERT_EQ(testClusters.WriteAttribute(context, request.request, decoder), CHIP_NO_ERROR);
        ASSERT_EQ(testClusters.GetBitmap8Value(), testValue);
    }
}
