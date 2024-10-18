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
#include <app/util/attribute-storage-null-handling.h>
#include <app/util/odd-sized-integers.h>
#include <lib/core/CHIPError.h>
#include <lib/core/TLVReader.h>
#include <lib/core/TLVTags.h>
#include <lib/core/TLVTypes.h>
#include <lib/core/TLVWriter.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/Span.h>

#include <limits>
#include <optional>

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

class EncodeResult
{
public:
    explicit EncodeResult() = default;
    EncodeResult(CHIP_ERROR error) : mResult(error) { VerifyOrDie(error != CHIP_NO_ERROR); }

    static EncodeResult Ok() { return EncodeResult(); }

    bool IsSuccess() const { return !mResult.has_value(); }

    bool operator==(const CHIP_ERROR & other) const { return mResult.has_value() && (*mResult == other); }

private:
    std::optional<CHIP_ERROR> mResult;
};

/// Validates that an encoded value in ember takes a specific format
class EncodeTester
{
public:
    EncodeTester(const EmberAfAttributeMetadata * meta) : mMetaData(meta) {}
    ~EncodeTester() = default;

    template <typename T, size_t N>
    EncodeResult TryEncode(const T & value, const uint8_t (&arr)[N])
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
            return err;
        }

        if (expected.size() != out_span.size())
        {
            ChipLogError(Test, "Decode mismatch in size: expected %u, got %u", static_cast<unsigned>(expected.size()),
                         static_cast<unsigned>(out_span.size()));
            return CHIP_ERROR_INTERNAL;
        }

        if (!expected.data_equal(out_span))
        {
            ChipLogError(Test, "Decode mismatch in content for %u bytes", static_cast<unsigned>(expected.size()));
            return CHIP_ERROR_INTERNAL;
        }

        return EncodeResult::Ok();
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
        .size          = 0, // likely not valid, however not used for tests
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

        EXPECT_TRUE(tester.TryEncode<uint8_t>(0, { 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint8_t>(123, { 123 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint8_t>(0xFD, { 0xFD }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint8_t>(255, { 0xFF }).IsSuccess());
    }

    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT8U_ATTRIBUTE_TYPE, true /* nullable */));

        EXPECT_TRUE(tester.TryEncode<uint8_t>(0, { 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint8_t>(123, { 123 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint8_t>(0xFD, { 0xFD }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<DataModel::Nullable<uint8_t>>(DataModel::NullNullable, { 0xFF }).IsSuccess());

        // Not allowed to encode null-equivalent
        EXPECT_EQ(tester.TryEncode<uint8_t>(0xFF, { 0xFF }), CHIP_IM_GLOBAL_STATUS(ConstraintError));
    }

    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT16U_ATTRIBUTE_TYPE, false /* nullable */));

        EXPECT_TRUE(tester.TryEncode<uint16_t>(0, { 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint16_t>(123, { 123, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint16_t>(0xFD, { 0xFD, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint16_t>(255, { 0xFF, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint16_t>(0xABCD, { 0xCD, 0xAB }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint16_t>(0xFFFF, { 0xFF, 0xFF }).IsSuccess());
    }

    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT16U_ATTRIBUTE_TYPE, true /* nullable */));

        EXPECT_TRUE(tester.TryEncode<uint16_t>(0, { 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint16_t>(123, { 123, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint16_t>(0xFD, { 0xFD, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint16_t>(255, { 0xFF, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint16_t>(0xABCD, { 0xCD, 0xAB }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<DataModel::Nullable<uint16_t>>(DataModel::NullNullable, { 0xFF, 0xFF }).IsSuccess());

        // Not allowed to encode null-equivalent
        EXPECT_EQ(tester.TryEncode<uint16_t>(0xFFFF, { 0xFF, 0xFF }), CHIP_IM_GLOBAL_STATUS(ConstraintError));
    }
    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT64U_ATTRIBUTE_TYPE, true /* nullable */));

        EXPECT_TRUE(tester.TryEncode<uint64_t>(0, { 0, 0, 0, 0, 0, 0, 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint64_t>(0x1234567, { 0x67, 0x45, 0x23, 0x01, 0, 0, 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint64_t>(0xAABBCCDDEEFF1122, { 0x22, 0x11, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA }).IsSuccess());
        EXPECT_TRUE(tester
                        .TryEncode<DataModel::Nullable<uint64_t>>(DataModel::NullNullable,
                                                                  { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF })
                        .IsSuccess());
    }

    /// Odd sized integers
    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT24U_ATTRIBUTE_TYPE, false /* nullable */));
        EXPECT_TRUE(tester.TryEncode<uint32_t>(0, { 0, 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint32_t>(0x123456, { 0x56, 0x34, 0x12 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint32_t>(0xFFFFFF, { 0xFF, 0xFF, 0xFF }).IsSuccess());

        // Out of range
        EXPECT_EQ(tester.TryEncode<uint32_t>(0x1000000, { 0 }), CHIP_ERROR_INVALID_ARGUMENT);
        EXPECT_EQ(tester.TryEncode<uint32_t>(0xFF000000, { 0 }), CHIP_ERROR_INVALID_ARGUMENT);
    }
    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT24U_ATTRIBUTE_TYPE, true /* nullable */));
        EXPECT_TRUE(tester.TryEncode<uint32_t>(0, { 0, 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint32_t>(0x123456, { 0x56, 0x34, 0x12 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<DataModel::Nullable<uint32_t>>(DataModel::NullNullable, { 0xFF, 0xFF, 0xFF }).IsSuccess());

        // Out of range
        EXPECT_EQ(tester.TryEncode<uint32_t>(0x1000000, { 0 }), CHIP_ERROR_INVALID_ARGUMENT);
        // cannot encode null equivalent value
        EXPECT_EQ(tester.TryEncode<uint32_t>(0xFFFFFF, { 0x56, 0x34, 0x12 }), CHIP_ERROR_INVALID_ARGUMENT);
    }

    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT40U_ATTRIBUTE_TYPE, true /* nullable */));
        EXPECT_TRUE(tester.TryEncode<uint64_t>(0, { 0, 0, 0, 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint64_t>(0x123456, { 0x56, 0x34, 0x12, 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<uint64_t>(0x123456FFFF, { 0xFF, 0xFF, 0x56, 0x34, 0x12 }).IsSuccess());
        EXPECT_TRUE(
            tester.TryEncode<DataModel::Nullable<uint64_t>>(DataModel::NullNullable, { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }).IsSuccess());

        // Out of range
        EXPECT_EQ(tester.TryEncode<uint64_t>(0x10011001100, { 0 }), CHIP_ERROR_INVALID_ARGUMENT);
        // cannot encode null equivalent value
        EXPECT_EQ(tester.TryEncode<uint64_t>(0xFFFFFFFFFF, { 0 }), CHIP_ERROR_INVALID_ARGUMENT);
    }
}

TEST(TestEmberAttributeBuffer, TestEncodeSignedTypes)
{
    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT8S_ATTRIBUTE_TYPE, false /* nullable */));

        EXPECT_TRUE(tester.TryEncode<int8_t>(0, { 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int8_t>(123, { 123 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int8_t>(127, { 127 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int8_t>(-10, { 0xF6 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int8_t>(-128, { 0x80 }).IsSuccess());
    }

    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT8S_ATTRIBUTE_TYPE, true /* nullable */));

        EXPECT_TRUE(tester.TryEncode<int8_t>(0, { 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int8_t>(123, { 123 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int8_t>(127, { 127 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int8_t>(-10, { 0xF6 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int8_t>(-127, { 0x81 }).IsSuccess());

        // NULL canot be encoded
        EXPECT_EQ(tester.TryEncode<int8_t>(std::numeric_limits<int8_t>::min(), { 0x80 }), CHIP_IM_GLOBAL_STATUS(ConstraintError));
    }
    {

        EncodeTester tester(CreateFakeMeta(ZCL_INT16S_ATTRIBUTE_TYPE, false /* nullable */));

        EXPECT_TRUE(tester.TryEncode<int16_t>(0, { 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int16_t>(123, { 123, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int16_t>(127, { 127, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int16_t>(-10, { 0xF6, 0xFF }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int16_t>(-128, { 0x80, 0xFF }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int16_t>(-1234, { 0x2E, 0xFB }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int16_t>(std::numeric_limits<int16_t>::min(), { 0x0, 0x80 }).IsSuccess());
    }

    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT16S_ATTRIBUTE_TYPE, true /* nullable */));

        EXPECT_TRUE(tester.TryEncode<int16_t>(0, { 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int16_t>(123, { 123, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int16_t>(127, { 127, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int16_t>(-10, { 0xF6, 0xFF }).IsSuccess());

        // NULL canot be encoded
        EXPECT_EQ(tester.TryEncode<int16_t>(std::numeric_limits<int16_t>::min(), { 0x80 }), CHIP_IM_GLOBAL_STATUS(ConstraintError));
    }

    // Odd size integers
    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT24S_ATTRIBUTE_TYPE, false /* nullable */));

        EXPECT_TRUE(tester.TryEncode<int32_t>(0, { 0, 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int32_t>(0x123456, { 0x56, 0x34, 0x12 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int32_t>(-1, { 0xFF, 0xFF, 0xFF }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int32_t>(-10, { 0xF6, 0xFF, 0xFF }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int32_t>(-1234, { 0x2E, 0xFB, 0xFF }).IsSuccess());

        // Out of range
        EXPECT_EQ(tester.TryEncode<int32_t>(0x1000000, { 0 }), CHIP_ERROR_INVALID_ARGUMENT);
        EXPECT_EQ(tester.TryEncode<int32_t>(0x0F000000, { 0 }), CHIP_ERROR_INVALID_ARGUMENT);
        EXPECT_EQ(tester.TryEncode<int32_t>(-0x1000000, { 0 }), CHIP_ERROR_INVALID_ARGUMENT);
    }
    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT24S_ATTRIBUTE_TYPE, true /* nullable */));

        EXPECT_TRUE(tester.TryEncode<int32_t>(0, { 0, 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int32_t>(0x123456, { 0x56, 0x34, 0x12 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int32_t>(-1, { 0xFF, 0xFF, 0xFF }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int32_t>(-10, { 0xF6, 0xFF, 0xFF }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int32_t>(-1234, { 0x2E, 0xFB, 0xFF }).IsSuccess());

        EXPECT_TRUE(tester.TryEncode<DataModel::Nullable<uint32_t>>(DataModel::NullNullable, { 0x00, 0x00, 0x80 }).IsSuccess());

        // Out of range
        EXPECT_EQ(tester.TryEncode<int32_t>(0x1000000, { 0 }), CHIP_ERROR_INVALID_ARGUMENT);
        // cannot encode null equivalent value
        EXPECT_EQ(tester.TryEncode<int32_t>(0xFFFFFF, { 0x56, 0x34, 0x12 }), CHIP_ERROR_INVALID_ARGUMENT);
    }

    {
        EncodeTester tester(CreateFakeMeta(ZCL_INT40S_ATTRIBUTE_TYPE, true /* nullable */));

        // NOTE: to generate encoded values, you an use commands like:
        //
        //    python -c 'import struct; print(", ".join(["0x%X" % v for v in struct.pack("<q", -12345678910)]))'
        //
        //    OUTPUT: 0xC2, 0xE3, 0x23, 0x20, 0xFD, 0xFF, 0xFF, 0xFF
        //
        EXPECT_TRUE(tester.TryEncode<int64_t>(0, { 0, 0, 0, 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int64_t>(0x123456, { 0x56, 0x34, 0x12, 0, 0 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int64_t>(-1234, { 0x2E, 0xFB, 0xFF, 0xFF, 0xFF }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int64_t>(-123456789, { 0xeb, 0x32, 0xa4, 0xf8, 0xFF }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<int64_t>(-12345678910, { 0xc2, 0xe3, 0x23, 0x20, 0xfd }).IsSuccess());

        EXPECT_TRUE(
            tester.TryEncode<DataModel::Nullable<uint64_t>>(DataModel::NullNullable, { 0x00, 0x00, 0x00, 0x00, 0x80 }).IsSuccess());

        // Out of range
        EXPECT_EQ(tester.TryEncode<int64_t>(0x10011001100, { 0 }), CHIP_ERROR_INVALID_ARGUMENT);
        // cannot encode null equivalent value
        EXPECT_EQ(tester.TryEncode<int64_t>(0xFFFFFFFFFF, { 0 }), CHIP_ERROR_INVALID_ARGUMENT);
        EXPECT_EQ(tester.TryEncode<int64_t>(-0x10000000000, { 0 }), CHIP_ERROR_INVALID_ARGUMENT);
    }
}

TEST(TestEmberAttributeBuffer, TestEncodeBool)
{
    {
        EncodeTester tester(CreateFakeMeta(ZCL_BOOLEAN_ATTRIBUTE_TYPE, false /* nullable */));

        EXPECT_TRUE(tester.TryEncode<bool>(true, { 1 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<bool>(false, { 0 }).IsSuccess());
    }

    {
        EncodeTester tester(CreateFakeMeta(ZCL_BOOLEAN_ATTRIBUTE_TYPE, true /* nullable */));

        EXPECT_TRUE(tester.TryEncode<bool>(true, { 1 }).IsSuccess());
        EXPECT_TRUE(tester.TryEncode<bool>(false, { 0 }).IsSuccess());
        EXPECT_TRUE(
            tester.TryEncode<DataModel::Nullable<bool>>(DataModel::NullNullable, { 0xFF}).IsSuccess());
    }
}
