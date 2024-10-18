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
#include <pw_unit_test/framework.h>

#include <app/codegen-data-model-provider/EmberDataBuffer.h>

#include <app-common/zap-generated/attribute-type.h>
#include <app/MessageDef/AttributeDataIB.h>
#include <app/data-model/Encode.h>
#include <app/data-model/Nullable.h>
#include <app/util/attribute-metadata.h>
#include <lib/core/CHIPError.h>
#include <lib/core/TLVReader.h>
#include <lib/core/TLVTags.h>
#include <lib/core/TLVTypes.h>
#include <lib/core/TLVWriter.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/Span.h>

#include <vector>

using namespace chip;
using namespace chip::app;

namespace {

/// encodes a simple value in a TLV buffer
class TLVEncodedValue
{
public:
    TLVEncodedValue()  = default;
    ~TLVEncodedValue() = default;

    template <typename T>
    TLV::TLVReader EncodeValue(const T & value)
    {
        const auto kTag = TLV::ContextTag(AttributeDataIB::Tag::kData);

        TLV::TLVWriter writer;
        writer.Init(mBuffer, sizeof(mBuffer));

        TLV::TLVType outer;

        VerifyOrDie(writer.StartContainer(TLV::AnonymousTag(), TLV::kTLVType_Structure, outer) == CHIP_NO_ERROR);
        VerifyOrDie(DataModel::Encode(writer, kTag, value) == CHIP_NO_ERROR);
        VerifyOrDie(writer.EndContainer(outer) == CHIP_NO_ERROR);

        VerifyOrDie(writer.Finalize() == CHIP_NO_ERROR);
        size_t fill = writer.GetLengthWritten();

        TLV::TLVReader reader;
        reader.Init(mBuffer, fill);
        VerifyOrDie(reader.Next() == CHIP_NO_ERROR);
        VerifyOrDie(reader.GetTag() == TLV::AnonymousTag());
        VerifyOrDie(reader.EnterContainer(outer) == CHIP_NO_ERROR);
        VerifyOrDie(reader.Next() == CHIP_NO_ERROR);
        VerifyOrDie(reader.GetTag() == kTag);

        return reader;
    }

private:
    static constexpr size_t kMaxSize = 128;
    uint8_t mBuffer[kMaxSize];
};

/// Validates that an encoded value in ember takes a specific format
class EncodeTester
{
public:
    EncodeTester(const EmberAfAttributeMetadata * meta) : mMetaData(meta) {}
    ~EncodeTester() = default;

    template <typename T, size_t N>
    bool EncodingOk(const T & value, const uint8_t (&arr)[N])
    {
        ByteSpan expected(arr);
        MutableByteSpan out_span(mEmberDataBuffer);
        Ember::EmberAttributeBuffer buffer(mMetaData, out_span);

        TLVEncodedValue tlvEncoded;
        TLV::TLVReader reader = tlvEncoded.EncodeValue(value);

        CHIP_ERROR err = buffer.Decode(reader);
        if (err != CHIP_NO_ERROR)
        {
            ChipLogError(Test, "Decoding failed: %" CHIP_ERROR_FORMAT, err.Format());
            return false;
        }

        if (expected.size() != out_span.size())
        {
            ChipLogError(Test, "Decode mismatch in size: expected %u, got %u", static_cast<unsigned>(expected.size()),
                         static_cast<unsigned>(out_span.size()));
            return false;
        }

        if (!expected.data_equal(out_span))
        {
            ChipLogError(Test, "Decode mismatch in content for %u bytes", static_cast<unsigned>(expected.size()));
            return false;
        }

        return true;
    }

private:
    static constexpr size_t kMaxSize = 128;
    const EmberAfAttributeMetadata * mMetaData;
    uint8_t mEmberDataBuffer[kMaxSize];
};

const EmberAfAttributeMetadata * CreateFakeMeta(EmberAfAttributeType type, bool nullable)
{
    static EmberAfAttributeMetadata meta = {
        .defaultValue  = EmberAfDefaultOrMinMaxAttributeValue(static_cast<uint8_t *>(nullptr)),
        .attributeId   = 0,
        .size          = 0, // likely not valid
        .attributeType = ZCL_UNKNOWN_ATTRIBUTE_TYPE,
        .mask          = 0,
    };

    meta.attributeType = type;
    meta.mask          = nullable ? ATTRIBUTE_MASK_NULLABLE : 0;

    return &meta;
}

} // namespace


// All the tests below assume buffer ordering in little endian format
// Since currently all chip platforms in CI are little endian, we just kept tests
// as-is
static_assert(!CHIP_CONFIG_BIG_ENDIAN_TARGET);

TEST(TestEmberAttributeBuffer, TestEncodeUnsignedTypes)
{
    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT8U_ATTRIBUTE_TYPE, false /* nullable */));

        ASSERT_TRUE(tester.EncodingOk<uint8_t>(0, { 0 }));
        ASSERT_TRUE(tester.EncodingOk<uint8_t>(123, { 123 }));
        ASSERT_TRUE(tester.EncodingOk<uint8_t>(0xFD, { 0xFD }));
        ASSERT_TRUE(tester.EncodingOk<uint8_t>(255, { 0xFF }));
    }

    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT8U_ATTRIBUTE_TYPE, true /* nullable */));

        ASSERT_TRUE(tester.EncodingOk<uint8_t>(0, { 0 }));
        ASSERT_TRUE(tester.EncodingOk<uint8_t>(123, { 123 }));
        ASSERT_TRUE(tester.EncodingOk<uint8_t>(0xFD, { 0xFD }));
        ASSERT_TRUE(tester.EncodingOk<DataModel::Nullable<uint8_t>>(DataModel::NullNullable, { 0xFF }));
    }

    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT16U_ATTRIBUTE_TYPE, false /* nullable */));

        ASSERT_TRUE(tester.EncodingOk<uint16_t>(0, { 0, 0 }));
        ASSERT_TRUE(tester.EncodingOk<uint16_t>(123, { 123, 0 }));
        ASSERT_TRUE(tester.EncodingOk<uint16_t>(0xFD, { 0xFD, 0 }));
        ASSERT_TRUE(tester.EncodingOk<uint16_t>(255, { 0xFF, 0 }));
        ASSERT_TRUE(tester.EncodingOk<uint16_t>(0xABCD, { 0xCD, 0xAB }));
        ASSERT_TRUE(tester.EncodingOk<uint16_t>(0xFFFF, { 0xFF, 0xFF }));
    }

    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT16U_ATTRIBUTE_TYPE, true /* nullable */));

        ASSERT_TRUE(tester.EncodingOk<uint16_t>(0, { 0, 0 }));
        ASSERT_TRUE(tester.EncodingOk<uint16_t>(123, { 123, 0 }));
        ASSERT_TRUE(tester.EncodingOk<uint16_t>(0xFD, { 0xFD, 0 }));
        ASSERT_TRUE(tester.EncodingOk<uint16_t>(255, { 0xFF, 0 }));
        ASSERT_TRUE(tester.EncodingOk<uint16_t>(0xABCD, { 0xCD, 0xAB }));
        ASSERT_TRUE(tester.EncodingOk<DataModel::Nullable<uint16_t>>(DataModel::NullNullable, { 0xFF, 0xFF }));
    }
    {

        EncodeTester tester(CreateFakeMeta(ZCL_INT64U_ATTRIBUTE_TYPE, true /* nullable */));

        ASSERT_TRUE(tester.EncodingOk<uint64_t>(0, { 0, 0, 0, 0, 0, 0, 0, 0 }));
        ASSERT_TRUE(tester.EncodingOk<uint64_t>(0x1234567, { 0x67, 0x45, 0x23, 0x01, 0, 0, 0, 0 }));
        ASSERT_TRUE(tester.EncodingOk<uint64_t>(0xAABBCCDDEEFF1122, { 0x22, 0x11, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA }));
        ASSERT_TRUE(tester.EncodingOk<DataModel::Nullable<uint64_t>>(DataModel::NullNullable, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }));
    }

}
