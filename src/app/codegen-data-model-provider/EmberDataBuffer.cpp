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
#include "protocols/interaction_model/StatusCode.h"
#include <app/codegen-data-model-provider/EmberDataBuffer.h>

#include <app-common/zap-generated/attribute-type.h>
#include <lib/core/CHIPError.h>
#include <limits>
#include <protocols/interaction_model/Constants.h>

namespace chip {
namespace app {
namespace Ember {

namespace {

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

    static constexpr SignedDecodeInfo OfSize(unsigned n)
    {
        return SignedDecodeInfo{
            n,
            -(1LL << (8 * n - 1)),
            ((1LL << (8 * n - 1)) - 1),
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
        return SignedDecodeInfo::OfSize(3);
    case ZCL_INT32S_ATTRIBUTE_TYPE: // Unsigned 32-bit integer
        return SignedDecodeInfo::From<int32_t>();
    case ZCL_INT40S_ATTRIBUTE_TYPE: // Unsigned 40-bit integer
        return SignedDecodeInfo::OfSize(5);
    case ZCL_INT48S_ATTRIBUTE_TYPE: // Unsigned 48-bit integer
        return SignedDecodeInfo::OfSize(6);
    case ZCL_INT56S_ATTRIBUTE_TYPE: // Unsigned 56-bit integer
        return SignedDecodeInfo::OfSize(7);
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

CHIP_ERROR EmberAttributeBuffer::DecodeUnsignedInteger(chip::TLV::TLVReader & reader)
{
    UnsignedDecodeInfo info = GetUnsignedDecodeInfo(mAttributeType);
    VerifyOrReturnError(mDataBuffer.size() >= info.byteCount, CHIP_ERROR_NO_MEMORY);

    // Any size of integer can be read by TLV getting 64-bit integers
    uint64_t value;

    if (reader.GetType() == TLV::kTLVType_Null)
    {
        if (!mIsNullable)
        {
            return CHIP_ERROR_WRONG_TLV_TYPE;
        }
        value = ~(0ULL); // Null value is ALWAYS negative-fill
    }
    else
    {
        ReturnErrorOnFailure(reader.Get(value));

        VerifyOrReturnError(mIsNullable // ::max() on the type is used as the NULL flag
                                ? (value < info.maxValue)
                                : (value <= info.maxValue),
                            OutOfRangeError(info.byteCount));
    }

#if CHIP_CONFIG_BIG_ENDIAN_TARGET
    for (unsigned i = info.byteCount; i > 0; i--)
    {
        mDataBuffer[i - 1] = (value & 0xFF);
        value >>= 8;
    }
#else
    memcpy(mDataBuffer.data(), &value, info.byteCount);
#endif

    mDataBuffer.reduce_size(info.byteCount);

    return CHIP_NO_ERROR;
}

CHIP_ERROR EmberAttributeBuffer::DecodeSignedInteger(chip::TLV::TLVReader & reader)
{
    SignedDecodeInfo info = GetSignedDecodeInfo(mAttributeType);
    VerifyOrReturnError(mDataBuffer.size() >= info.byteCount, CHIP_ERROR_NO_MEMORY);

    // Any size of integer can be read by TLV getting 64-bit integers
    int64_t value;

    if (reader.GetType() == TLV::kTLVType_Null)
    {
        if (!mIsNullable)
        {
            return CHIP_ERROR_WRONG_TLV_TYPE;
        }
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

#if CHIP_CONFIG_BIG_ENDIAN_TARGET
    for (unsigned i = info.byteCount; i > 0; i--)
    {
        mDataBuffer[i - 1] = (value & 0xFF);
        value >>= 8;
    }
#else
    memcpy(mDataBuffer.data(), &value, info.byteCount);
#endif

    mDataBuffer.reduce_size(info.byteCount);

    return CHIP_NO_ERROR;
}

CHIP_ERROR EmberAttributeBuffer::Decode(chip::TLV::TLVReader & reader)
{
    switch (mAttributeType)
    {
    case ZCL_BOOLEAN_ATTRIBUTE_TYPE: { // Boolean
        VerifyOrReturnError(mDataBuffer.size() > 0, CHIP_ERROR_NO_MEMORY);

        // Boolean values:
        //   0xFF is NULL
        //   0x01 is TRUE
        //   0x02 is FALSE
        if (reader.GetType() == TLV::kTLVType_Null)
        {
            if (!mIsNullable)
            {
                return CHIP_ERROR_WRONG_TLV_TYPE;
            }
            mDataBuffer[0] = 0xFF;
        }
        else
        {

            bool value;
            ReturnErrorOnFailure(reader.Get(value));
            mDataBuffer[0] = value ? 1 : 0;
        }

        mDataBuffer.reduce_size(1);
        return CHIP_NO_ERROR;
    }
    case ZCL_INT8U_ATTRIBUTE_TYPE:  // Unsigned 8-bit integer
    case ZCL_INT16U_ATTRIBUTE_TYPE: // Unsigned 16-bit integer
    case ZCL_INT24U_ATTRIBUTE_TYPE: // Unsigned 24-bit integer
    case ZCL_INT32U_ATTRIBUTE_TYPE: // Unsigned 32-bit integer
    case ZCL_INT40U_ATTRIBUTE_TYPE: // Unsigned 40-bit integer
    case ZCL_INT48U_ATTRIBUTE_TYPE: // Unsigned 48-bit integer
    case ZCL_INT56U_ATTRIBUTE_TYPE: // Unsigned 56-bit integer
    case ZCL_INT64U_ATTRIBUTE_TYPE: // Unsigned 64-bit integer
        return DecodeUnsignedInteger(reader);
    case ZCL_INT8S_ATTRIBUTE_TYPE:  // Signed 8-bit integer
    case ZCL_INT16S_ATTRIBUTE_TYPE: // Signed 16-bit integer
    case ZCL_INT24S_ATTRIBUTE_TYPE: // Signed 24-bit integer
    case ZCL_INT32S_ATTRIBUTE_TYPE: // Signed 32-bit integer
    case ZCL_INT40S_ATTRIBUTE_TYPE: // Signed 40-bit integer
    case ZCL_INT48S_ATTRIBUTE_TYPE: // Signed 48-bit integer
    case ZCL_INT56S_ATTRIBUTE_TYPE: // Signed 56-bit integer
    case ZCL_INT64S_ATTRIBUTE_TYPE: // Signed 64-bit integer
        return DecodeSignedInteger(reader);
    }

    // FIXME: implement
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

} // namespace Ember
} // namespace app
} // namespace chip
