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
#include <app/codegen-data-model-provider/EmberDataBuffer.h>

#include <app-common/zap-generated/attribute-type.h>
#include <app/util/attribute-storage-null-handling.h>
#include <cstdint>
#include <lib/core/CHIPError.h>
#include <lib/core/TLVTypes.h>
#include <lib/support/CodeUtils.h>
#include <protocols/interaction_model/Constants.h>
#include <protocols/interaction_model/StatusCode.h>

#include <limits>

namespace chip {
namespace app {
namespace Ember {

namespace {

/// Maximum length of a string, inclusive
///
/// the max size value (0xFF and 0xFFFF) is reserved for NULL representation so
/// it is not available
constexpr uint32_t MaxLength(EmberAttributeBuffer::PascalStringType s)
{
    if (s == EmberAttributeBuffer::PascalStringType::kShort)
    {
        return std::numeric_limits<uint8_t>::max() - 1;
    }
    // EmberAttributeBuffer::PascalStringType::kLong:
    return std::numeric_limits<uint16_t>::max() - 1;
}

struct UnsignedDecodeInfo
{
    unsigned byteCount;
    uint64_t maxValue;
};

constexpr UnsignedDecodeInfo GetUnsignedDecodeInfo(EmberAfAttributeType type)
{

    switch (type)
    {
    case ZCL_INT8U_ATTRIBUTE_TYPE: // Unsigned 8-bit integer
        return UnsignedDecodeInfo{ 1, 0xFF };
    case ZCL_INT16U_ATTRIBUTE_TYPE: // Unsigned 16-bit integer
        return UnsignedDecodeInfo{ 2, 0xFFFF };
    case ZCL_INT24U_ATTRIBUTE_TYPE: // Unsigned 24-bit integer
        return UnsignedDecodeInfo{ 3, 0xFFFFFF };
    case ZCL_INT32U_ATTRIBUTE_TYPE: // Unsigned 32-bit integer
        return UnsignedDecodeInfo{ 4, 0xFFFFFFFF };
    case ZCL_INT40U_ATTRIBUTE_TYPE: // Unsigned 40-bit integer
        return UnsignedDecodeInfo{ 5, 0xFFFFFFFFFF };
    case ZCL_INT48U_ATTRIBUTE_TYPE: // Unsigned 48-bit integer
        return UnsignedDecodeInfo{ 6, 0xFFFFFFFFFFFF };
    case ZCL_INT56U_ATTRIBUTE_TYPE: // Unsigned 56-bit integer
        return UnsignedDecodeInfo{ 7, 0xFFFFFFFFFFFFFF };
    case ZCL_INT64U_ATTRIBUTE_TYPE: // Unsigned 64-bit integer
        return UnsignedDecodeInfo{ 8, 0xFFFFFFFFFFFFFFFF };
    }
    chipDie();
}

struct SignedDecodeInfo
{
    unsigned byteCount;
    int64_t minValue;
    int64_t maxValue;

    template <typename T>
    static constexpr SignedDecodeInfo From()
    {
        return SignedDecodeInfo{
            sizeof(T),
            std::numeric_limits<T>::min(),
            std::numeric_limits<T>::max(),
        };
    }

    template <size_t N>
    static constexpr SignedDecodeInfo OfSize()
    {
        // I specifically do not try to support "8" due to overflows.
        static_assert(N > 0 && N < 8, "This covers mostly odd-sized integers only");
        return SignedDecodeInfo{
            N,
            -static_cast<int64_t>(1LL << (8 * N - 1)),
            (static_cast<int64_t>((1LL << (8 * N - 1))) - 1),
        };
    }
};

constexpr SignedDecodeInfo GetSignedDecodeInfo(EmberAfAttributeType type)
{

    switch (type)
    {
    case ZCL_INT8S_ATTRIBUTE_TYPE: // Unsigned 8-bit integer
        return SignedDecodeInfo::From<int8_t>();
    case ZCL_INT16S_ATTRIBUTE_TYPE: // Unsigned 16-bit integer
        return SignedDecodeInfo::From<int16_t>();
    case ZCL_INT24S_ATTRIBUTE_TYPE: // Unsigned 24-bit integer
        return SignedDecodeInfo::OfSize<3>();
    case ZCL_INT32S_ATTRIBUTE_TYPE: // Unsigned 32-bit integer
        return SignedDecodeInfo::From<int32_t>();
    case ZCL_INT40S_ATTRIBUTE_TYPE: // Unsigned 40-bit integer
        return SignedDecodeInfo::OfSize<5>();
    case ZCL_INT48S_ATTRIBUTE_TYPE: // Unsigned 48-bit integer
        return SignedDecodeInfo::OfSize<6>();
    case ZCL_INT56S_ATTRIBUTE_TYPE: // Unsigned 56-bit integer
        return SignedDecodeInfo::OfSize<7>();
    case ZCL_INT64S_ATTRIBUTE_TYPE: // Unsigned 64-bit integer
        return SignedDecodeInfo::From<int64_t>();
    }
    chipDie();
}

static constexpr bool IsOddIntegerSize(unsigned byteCount)
{
    // All these conditions seem to result in the same code size:
    // - (byteCount > 2) && (byteCount != 4) && (byteCount != 8) OR
    // - (byteCount == 6) || ((byteCount & 0x1) != 0)
    //
    // So ended up keeping the "readable" one
    return (byteCount == 3) || (byteCount == 5) || (byteCount == 6) || (byteCount == 7);
}

// This is an odd workaround for legacy. Errors SHOULD be always ConstraintError
// however in practice old ember code returns INVALID_ARGUMENT for odd sized integers
//
// TODO: This should ALWAYS return ConstraintError (and method should not exist ...)
CHIP_ERROR OutOfRangeError(unsigned byteCount)
{
    if (IsOddIntegerSize(byteCount))
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }
    return CHIP_IM_GLOBAL_STATUS(ConstraintError);
}

} // namespace

CHIP_ERROR EmberAttributeBuffer::DecodeUnsignedInteger(chip::TLV::TLVReader & reader, EndianWriter & writer)
{
    UnsignedDecodeInfo info = GetUnsignedDecodeInfo(mAttributeType);

    // Any size of integer can be read by TLV getting 64-bit integers
    uint64_t value;

    if (reader.GetType() == TLV::kTLVType_Null)
    {
        // we know mIsNullable due to the check at the top of ::Decode
        // NULL is alwayx 0xFFFFF....FFF so we just set to max
        value = std::numeric_limits<uint64_t>::max();
    }
    else
    {
        ReturnErrorOnFailure(reader.Get(value));

        VerifyOrReturnError(mIsNullable // ::max() on the type is used as the NULL flag
                                ? (value < info.maxValue)
                                : (value <= info.maxValue),
                            OutOfRangeError(info.byteCount));
    }

    writer.EndianPut(value, info.byteCount);
    return CHIP_NO_ERROR;
}

CHIP_ERROR EmberAttributeBuffer::DecodeSignedInteger(chip::TLV::TLVReader & reader, EndianWriter & writer)
{
    SignedDecodeInfo info = GetSignedDecodeInfo(mAttributeType);

    // Any size of integer can be read by TLV getting 64-bit integers
    int64_t value;

    if (reader.GetType() == TLV::kTLVType_Null)
    {
        // we know mIsNullable due to the check at the top of ::Decode

        // Most negative integer (i.e. 0b1000...0 is flagged as NULL value)
        value = info.minValue;
    }
    else
    {
        ReturnErrorOnFailure(reader.Get(value));

        // NULLABLE reserves minValue for NULL, so range is:
        //   - NULLABLE:      (minValue, MaxValue]
        //   - NON-NULLABLE:  [minValue, MaxValue]
        bool valid = (value <= info.maxValue) && (mIsNullable ? (value > info.minValue) : (value >= info.minValue));

        VerifyOrReturnError(valid, OutOfRangeError(info.byteCount));
    }
    writer.EndianPutSigned(value, info.byteCount);
    return CHIP_NO_ERROR;
}

CHIP_ERROR EmberAttributeBuffer::DecodeAsString(chip::TLV::TLVReader & reader, PascalStringType stringType, TLV::TLVType tlvType,
                                                EndianWriter & writer)
{
    // Handle null first, then the actual data
    if (reader.GetType() == TLV::kTLVType_Null)
    {
        // we know mIsNullable due to the check at the top of ::Decode
        switch (stringType)
        {
        case PascalStringType::kShort:
            writer.Put8(NumericAttributeTraits<uint8_t>::kNullValue);
            break;
        case PascalStringType::kLong:
            writer.Put16(NumericAttributeTraits<uint16_t>::kNullValue);
            break;
        }
        return CHIP_NO_ERROR;
    }

    const uint32_t stringLength = reader.GetLength();

    VerifyOrReturnError(reader.GetType() == tlvType, CHIP_ERROR_WRONG_TLV_TYPE);
    VerifyOrReturnError(stringLength <= MaxLength(stringType), CHIP_ERROR_INVALID_ARGUMENT);

    // Size is a prefix, where 0xFF/0xFFFF is the null marker (if applicable)
    switch (stringType)
    {
    case PascalStringType::kShort:
        writer.Put8(static_cast<uint8_t>(stringLength));
        break;
    case PascalStringType::kLong:
        writer.Put16(static_cast<uint16_t>(stringLength));
        break;
    }

    // data copy
    const uint8_t * tlvData;
    ReturnErrorOnFailure(reader.GetDataPtr(tlvData));
    writer.Put(tlvData, stringLength);

    return CHIP_NO_ERROR;
}

CHIP_ERROR EmberAttributeBuffer::Decode(chip::TLV::TLVReader & reader)
{
    // all methods below assume that nullable setting matches  (this is to reduce code size
    // even though clarity suffers)
    VerifyOrReturnError(mIsNullable || reader.GetType() != TLV::kTLVType_Null, CHIP_ERROR_WRONG_TLV_TYPE);

    EndianWriter endianWriter(mDataBuffer.data(), mDataBuffer.size());

    switch (mAttributeType)
    {
    case ZCL_BOOLEAN_ATTRIBUTE_TYPE: // Boolean
        // Boolean values:
        //   0x00 is FALSE
        //   0x01 is TRUE
        //   0xFF is NULL
        if (reader.GetType() == TLV::kTLVType_Null)
        {
            // we know mIsNullable due to the check at the top of ::Decode
            endianWriter.Put8(NumericAttributeTraits<bool>::kNullValue);
        }
        else
        {
            bool value;
            ReturnErrorOnFailure(reader.Get(value));
            endianWriter.Put8(value ? 1 : 0);
        }
        break;
    case ZCL_INT8U_ATTRIBUTE_TYPE:  // Unsigned 8-bit integer
    case ZCL_INT16U_ATTRIBUTE_TYPE: // Unsigned 16-bit integer
    case ZCL_INT24U_ATTRIBUTE_TYPE: // Unsigned 24-bit integer
    case ZCL_INT32U_ATTRIBUTE_TYPE: // Unsigned 32-bit integer
    case ZCL_INT40U_ATTRIBUTE_TYPE: // Unsigned 40-bit integer
    case ZCL_INT48U_ATTRIBUTE_TYPE: // Unsigned 48-bit integer
    case ZCL_INT56U_ATTRIBUTE_TYPE: // Unsigned 56-bit integer
    case ZCL_INT64U_ATTRIBUTE_TYPE: // Unsigned 64-bit integer
        ReturnErrorOnFailure(DecodeUnsignedInteger(reader, endianWriter));
        break;
    case ZCL_INT8S_ATTRIBUTE_TYPE:  // Signed 8-bit integer
    case ZCL_INT16S_ATTRIBUTE_TYPE: // Signed 16-bit integer
    case ZCL_INT24S_ATTRIBUTE_TYPE: // Signed 24-bit integer
    case ZCL_INT32S_ATTRIBUTE_TYPE: // Signed 32-bit integer
    case ZCL_INT40S_ATTRIBUTE_TYPE: // Signed 40-bit integer
    case ZCL_INT48S_ATTRIBUTE_TYPE: // Signed 48-bit integer
    case ZCL_INT56S_ATTRIBUTE_TYPE: // Signed 56-bit integer
    case ZCL_INT64S_ATTRIBUTE_TYPE: // Signed 64-bit integer
        ReturnErrorOnFailure(DecodeSignedInteger(reader, endianWriter));
        break;
    case ZCL_SINGLE_ATTRIBUTE_TYPE: { // 32-bit float
        float value;
        if (reader.GetType() == TLV::kTLVType_Null)
        {
            // we know mIsNullable due to the check at the top of ::Decode
            NumericAttributeTraits<float>::SetNull(value);
        }
        else
        {

            ReturnErrorOnFailure(reader.Get(value));
        }
        endianWriter.Put(&value, sizeof(value));
        break;
    }
    case ZCL_DOUBLE_ATTRIBUTE_TYPE: { // 64-bit float
        double value;
        if (reader.GetType() == TLV::kTLVType_Null)
        {
            // we know mIsNullable due to the check at the top of ::Decode
            NumericAttributeTraits<double>::SetNull(value);
        }
        else
        {
            ReturnErrorOnFailure(reader.Get(value));
        }
        endianWriter.Put(&value, sizeof(value));
        break;
    }
    case ZCL_CHAR_STRING_ATTRIBUTE_TYPE: // Char string
        ReturnErrorOnFailure(DecodeAsString(reader, PascalStringType::kShort, TLV::kTLVType_UTF8String, endianWriter));
        break;
    case ZCL_LONG_CHAR_STRING_ATTRIBUTE_TYPE:
        ReturnErrorOnFailure(DecodeAsString(reader, PascalStringType::kLong, TLV::kTLVType_UTF8String, endianWriter));
        break;
    case ZCL_OCTET_STRING_ATTRIBUTE_TYPE: // Octet string
        ReturnErrorOnFailure(DecodeAsString(reader, PascalStringType::kShort, TLV::kTLVType_ByteString, endianWriter));
        break;
    case ZCL_LONG_OCTET_STRING_ATTRIBUTE_TYPE:
        ReturnErrorOnFailure(DecodeAsString(reader, PascalStringType::kLong, TLV::kTLVType_ByteString, endianWriter));
        break;
    default:
        ChipLogError(DataManagement, "Attribute type 0x%x not handled", mAttributeType);
        return CHIP_IM_GLOBAL_STATUS(Failure);
    }

    size_t written;
    if (!endianWriter.Fit(written))
    {
        return CHIP_ERROR_NO_MEMORY;
    }

    mDataBuffer.reduce_size(written);
    return CHIP_NO_ERROR;
}

CHIP_ERROR EmberAttributeBuffer::EncodeInteger(chip::TLV::TLVWriter & writer, TLV::Tag tag, EndianReader & reader) const
{
    // Encodes an integer by first reading as raw bytes and then
    // bitshift-convert
    //
    // This optimizes code size rather than readability at this point.

    uint8_t raw_bytes[8];

    bool isSigned = (mAttributeType == ZCL_INT8S_ATTRIBUTE_TYPE) //
        || (mAttributeType == ZCL_INT16S_ATTRIBUTE_TYPE)         //
        || (mAttributeType == ZCL_INT24S_ATTRIBUTE_TYPE)         //
        || (mAttributeType == ZCL_INT32S_ATTRIBUTE_TYPE)         //
        || (mAttributeType == ZCL_INT40S_ATTRIBUTE_TYPE)         //
        || (mAttributeType == ZCL_INT48S_ATTRIBUTE_TYPE)         //
        || (mAttributeType == ZCL_INT56S_ATTRIBUTE_TYPE)         //
        || (mAttributeType == ZCL_INT64S_ATTRIBUTE_TYPE);

    unsigned byteCount;
    uint64_t nullValue;

    if (isSigned)
    {
        const SignedDecodeInfo info = GetSignedDecodeInfo(mAttributeType);
        byteCount                   = info.byteCount;
        nullValue                   = static_cast<uint64_t>(info.minValue); // just a bit cast for easy compare
    }
    else
    {
        const UnsignedDecodeInfo info = GetUnsignedDecodeInfo(mAttributeType);
        byteCount                     = info.byteCount;
        nullValue                     = info.maxValue;
    }

    VerifyOrDie(sizeof(raw_bytes) >= byteCount);
    if (!reader.ReadBytes(raw_bytes, byteCount).IsSuccess())
    {
        return reader.StatusCode();
    }

    // At this point, RAW_VALUE contains the actual value, need to make it "real"
    union
    {
        int64_t int_value;
        uint64_t uint_value;
    } value;

    value.uint_value = 0;

#if CHIP_CONFIG_BIG_ENDIAN_TARGET
    bool isNegative = isSigned && (raw_bytes[0] >= 0x80);
    if (isNegative)
    {
        value.int_value = -1;
    }
    for (int i = 0; i < static_cast<int>(byteCount); i++)
    {
#else
    bool isNegative = isSigned && (raw_bytes[byteCount - 1] >= 0x80);
    if (isNegative)
    {
        value.int_value = -1;
    }
    for (int i = static_cast<int>(byteCount) - 1; i >= 0; i--)
    {
#endif
        value.uint_value <<= 8;
        value.uint_value = (value.uint_value & ~0xFFULL) | raw_bytes[i];
    }

    if (mIsNullable && (value.uint_value == nullValue))
    {
        // MaxValue is used for NULL setting
        return writer.PutNull(tag);
    }

    switch (mAttributeType)
    {
    case ZCL_INT8U_ATTRIBUTE_TYPE: // Unsigned 8-bit integer
        return writer.Put(tag, static_cast<uint8_t>(value.uint_value));
    case ZCL_INT16U_ATTRIBUTE_TYPE: // Unsigned 16-bit integer
        return writer.Put(tag, static_cast<uint16_t>(value.uint_value));
    case ZCL_INT24U_ATTRIBUTE_TYPE: // Unsigned 24-bit integer
    case ZCL_INT32U_ATTRIBUTE_TYPE: // Unsigned 32-bit integer
        return writer.Put(tag, static_cast<uint32_t>(value.uint_value));
    case ZCL_INT40U_ATTRIBUTE_TYPE: // Unsigned 40-bit integer
    case ZCL_INT48U_ATTRIBUTE_TYPE: // Unsigned 48-bit integer
    case ZCL_INT56U_ATTRIBUTE_TYPE: // Unsigned 56-bit integer
    case ZCL_INT64U_ATTRIBUTE_TYPE: // Unsigned 64-bit integer
        return writer.Put(tag, static_cast<uint64_t>(value.uint_value));
    case ZCL_INT8S_ATTRIBUTE_TYPE: // Unsigned 8-bit integer
        return writer.Put(tag, static_cast<int8_t>(value.int_value));
    case ZCL_INT16S_ATTRIBUTE_TYPE: // Unsigned 16-bit integer
        return writer.Put(tag, static_cast<int16_t>(value.int_value));
    case ZCL_INT24S_ATTRIBUTE_TYPE: // Unsigned 24-bit integer
    case ZCL_INT32S_ATTRIBUTE_TYPE: // Unsigned 32-bit integer
        return writer.Put(tag, static_cast<int32_t>(value.int_value));
    default:
        return writer.Put(tag, static_cast<int64_t>(value.int_value));
    }
}

CHIP_ERROR EmberAttributeBuffer::Encode(chip::TLV::TLVWriter & writer, TLV::Tag tag) const
{
    EndianReader endianReader(mDataBuffer.data(), mDataBuffer.size());

    switch (mAttributeType)
    {
    case ZCL_NO_DATA_ATTRIBUTE_TYPE: // No data
        return writer.PutNull(tag);
    case ZCL_BOOLEAN_ATTRIBUTE_TYPE: { // Boolean
        uint8_t value;
        if (!endianReader.Read8(&value).IsSuccess())
        {
            return endianReader.StatusCode();
        }
        switch (value)
        {
        case 0:
        case 1:
            return writer.PutBoolean(tag, value != 0);
        case 0xFF:
            return writer.PutNull(tag);
        default:
            // Unknown types
            return CHIP_ERROR_INCORRECT_STATE;
        }
    }
    case ZCL_INT8U_ATTRIBUTE_TYPE:  // Unsigned 8-bit integer
    case ZCL_INT16U_ATTRIBUTE_TYPE: // Unsigned 16-bit integer
    case ZCL_INT24U_ATTRIBUTE_TYPE: // Unsigned 24-bit integer
    case ZCL_INT32U_ATTRIBUTE_TYPE: // Unsigned 32-bit integer
    case ZCL_INT40U_ATTRIBUTE_TYPE: // Unsigned 40-bit integer
    case ZCL_INT48U_ATTRIBUTE_TYPE: // Unsigned 48-bit integer
    case ZCL_INT56U_ATTRIBUTE_TYPE: // Unsigned 56-bit integer
    case ZCL_INT64U_ATTRIBUTE_TYPE: // Unsigned 64-bit integer
    case ZCL_INT8S_ATTRIBUTE_TYPE:  // Signed 8-bit integer
    case ZCL_INT16S_ATTRIBUTE_TYPE: // Signed 16-bit integer
    case ZCL_INT24S_ATTRIBUTE_TYPE: // Signed 24-bit integer
    case ZCL_INT32S_ATTRIBUTE_TYPE: // Signed 32-bit integer
    case ZCL_INT40S_ATTRIBUTE_TYPE: // Signed 40-bit integer
    case ZCL_INT48S_ATTRIBUTE_TYPE: // Signed 48-bit integer
    case ZCL_INT56S_ATTRIBUTE_TYPE: // Signed 56-bit integer
    case ZCL_INT64S_ATTRIBUTE_TYPE: // Signed 64-bit integer
        return EncodeInteger(writer, tag, endianReader);
        // case ZCL_SINGLE_ATTRIBUTE_TYPE:      // 32-bit float
        // case ZCL_DOUBLE_ATTRIBUTE_TYPE:      // 64-bit float
        // case ZCL_CHAR_STRING_ATTRIBUTE_TYPE: // Char string
        // case ZCL_LONG_CHAR_STRING_ATTRIBUTE_TYPE:
        // case ZCL_OCTET_STRING_ATTRIBUTE_TYPE: // Octet string
        // case ZCL_LONG_OCTET_STRING_ATTRIBUTE_TYPE:
    }

    // FIXME: implement
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

} // namespace Ember
} // namespace app
} // namespace chip
