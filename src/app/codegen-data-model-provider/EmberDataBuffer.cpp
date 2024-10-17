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

constexpr bool IsNullValue(uint64_t v, unsigned byteCount)
{
    for (unsigned i = 0; i < byteCount; i++)
    {
        if ((v & 0xFF) != 0xFF)
        {
            return false;
        }
        v >>= 8;
    }
    return true;
}

} // namespace

CHIP_ERROR EmberAttributeBuffer::DecodeUnsigned(chip::TLV::TLVReader & reader)
{
    UnsignedDecodeInfo info = GetUnsignedDecodeInfo(mAttributeType);
    VerifyOrReturnError(mDataBuffer.size() >= info.byteCount, CHIP_ERROR_NO_MEMORY);

    // Any size of integer can be read by TLV getting 64-bit integers
    uint64_t value;

    if (mIsNullable && reader.GetType() == TLV::kTLVType_Null)
    {
        value = ~(0ULL); // Null value is ALWAYS negative-fill
    }
    else
    {
        ReturnErrorOnFailure(reader.Get(value));

        if (value > info.maxValue)
        {
            return CHIP_ERROR_INVALID_ARGUMENT;
        }

        if (mIsNullable)
        {
            VerifyOrReturnError(IsNullValue(value, info.byteCount), CHIP_ERROR_INVALID_ARGUMENT);
        }
    }

#if CHIP_CONFIG_BIG_ENDIAN_TARGET
    for (unsigned i = info.byteCount; i > 0; i--)
    {
        mDataBuffer[i - 1] = (value & 0xFF);
        value >>= 8;
    }
#else
    for (unsigned i = 0; i < info.byteCount; i++)
    {
        mDataBuffer[i] = (value & 0xFF);
        value >>= 8;
    }
#endif

    mDataBuffer.reduce_size(info.byteCount);

    return CHIP_NO_ERROR;
}

CHIP_ERROR EmberAttributeBuffer::Decode(chip::TLV::TLVReader & reader)
{
    switch (mAttributeType)
    {
    case ZCL_INT8U_ATTRIBUTE_TYPE:  // Unsigned 8-bit integer
    case ZCL_INT16U_ATTRIBUTE_TYPE: // Unsigned 16-bit integer
    case ZCL_INT24U_ATTRIBUTE_TYPE: // Unsigned 24-bit integer
    case ZCL_INT32U_ATTRIBUTE_TYPE: // Unsigned 32-bit integer
    case ZCL_INT40U_ATTRIBUTE_TYPE: // Unsigned 40-bit integer
    case ZCL_INT48U_ATTRIBUTE_TYPE: // Unsigned 48-bit integer
    case ZCL_INT56U_ATTRIBUTE_TYPE: // Unsigned 56-bit integer
    case ZCL_INT64U_ATTRIBUTE_TYPE: // Unsigned 64-bit integer
        return DecodeUnsigned(reader);
    }

    // FIXME: implement
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

} // namespace Ember
} // namespace app
} // namespace chip
