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
#pragma once

#include <lib/core/TLVTypes.h>
#include <app/util/attribute-metadata.h>
#include <app/util/ember-io-storage.h>
#include <lib/core/TLVReader.h>
#include <lib/support/Span.h>

namespace chip {
namespace app {
namespace Ember {

/// This class represents a pointer to an ember-encoded attribute in a specific memory location.
///
/// Ember attributes are stored as raw bytes for numeric types (i.e. memcpy-like storage except
/// unaligned) and strings are Pascal-like (short with 1-byte length prefix or long with 2-byte length
/// prefix).
class EmberAttributeBuffer
{
public:
    static constexpr bool kIsFabricScoped = false;

    EmberAttributeBuffer(const EmberAfAttributeMetadata * meta, MutableByteSpan & data) :
        mIsNullable(meta->IsNullable()), mAttributeType(chip::app::Compatibility::Internal::AttributeBaseType(meta->attributeType)),
        mDataBuffer(data)
    {}

    /// Reads the data pointed into by `reader` and updates the data
    /// internally into mDataBuffer (which is then reflected outwards)
    CHIP_ERROR Decode(chip::TLV::TLVReader & reader);

private:
    CHIP_ERROR DecodeUnsignedInteger(chip::TLV::TLVReader & reader);
    CHIP_ERROR DecodeSignedInteger(chip::TLV::TLVReader & reader);
    CHIP_ERROR DecodeAsString(chip::TLV::TLVReader & reader, unsigned size_length, TLV::TLVType stringType);

    const bool mIsNullable;
    const EmberAfAttributeType mAttributeType;
    MutableByteSpan & mDataBuffer;
};

} // namespace Ember

namespace DataModel {

/// Helper method to forward the decode of this type to the class specific implementation
inline CHIP_ERROR Decode(TLV::TLVReader & reader, Ember::EmberAttributeBuffer & buffer)
{
    return buffer.Decode(reader);
}

} // namespace DataModel
} // namespace app
} // namespace chip
