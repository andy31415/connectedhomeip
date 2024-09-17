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

#include "access/SubjectDescriptor.h"
#include "app/AttributeEncodeState.h"
#include "lib/core/DataModelTypes.h"
#include "lib/support/BitFlags.h"
#include <app/AttributeValueEncoder.h>
#include <app/ConcreteAttributePath.h>
#include <app/data-model-provider/OperationTypes.h>
#include <app/data-model-provider/tests/TestConstants.h>

#include <vector>

namespace chip {
namespace app {
namespace Testing {

/// Contains information about a single parsed item inside an attribute data IB
struct DecodedAttributeData
{
    chip::DataVersion dataVersion;
    chip::app::ConcreteDataAttributePath attributePath;
    chip::TLV::TLVReader dataReader;

    CHIP_ERROR DecodeFrom(const chip::app::AttributeDataIB::Parser & parser);
};

CHIP_ERROR DecodeAttributeReportIBs(ByteSpan data, std::vector<DecodedAttributeData> & decoded_items);

/// Maintains an internal TLV buffer for data encoding and
/// decoding for ReportIBs.
///
/// Main use case is that explicit TLV layouts (structure and container starting) need to be
/// prepared to have a proper AttributeReportIBs::Builder/parser to exist.
class EncodedReportIBs
{
public:
    /// Initialize the report structures required to encode a
    CHIP_ERROR StartEncoding(app::AttributeReportIBs::Builder & builder);
    CHIP_ERROR FinishEncoding(app::AttributeReportIBs::Builder & builder);

    /// Decode the embedded attribute report IBs.
    /// The TLVReaders inside data have a lifetime tied to the current object (its readers point
    /// inside the current object)
    CHIP_ERROR Decode(std::vector<DecodedAttributeData> & decoded_items) const;

private:
    uint8_t mTlvDataBuffer[1024];
    TLV::TLVType mOuterStructureType;
    TLV::TLVWriter mEncodeWriter;
    ByteSpan mDecodeSpan;
};

/// Contains a `ReadAttributeRequest` as well as classes to convert this into a AttributeReportIBs
/// and later decode it
///
/// It wraps boilerplate code to obtain a `AttributeValueEncoder` as well as later decoding
/// the underlying encoded data for verification.
class TestReadRequest
{
public:
    /// Represents parameters for StartEncoding
    class EncodingParams
    {
    public:
        EncodingParams() {}

        EncodingParams & SetDataVersion(chip::DataVersion v)
        {
            mDataVersion = v;
            return *this;
        }

        EncodingParams & SetIsFabricFiltered(bool filtered)
        {
            mIsFabricFiltered = filtered;
            return *this;
        }

        EncodingParams & SetEncodingState(const AttributeEncodeState & state)
        {
            mAttributeEncodeState = state;
            return *this;
        }

        chip::DataVersion GetDataVersion() const { return mDataVersion; }
        bool GetIsFabricFiltered() const { return mIsFabricFiltered; }
        const AttributeEncodeState & GetAttributeEncodeState() const { return mAttributeEncodeState; }

    private:
        chip::DataVersion mDataVersion = 0x1234;
        bool mIsFabricFiltered         = false;
        AttributeEncodeState mAttributeEncodeState;
    };

    /// Convenience builds an object by incrementally adding information
    class ConstructionArguments
    {
    public:
        ConstructionArguments(ConcreteAttributePath path) { mRequest.path = path; }

        ConstructionArguments & SetSubjectDescriptor(const chip::Access::SubjectDescriptor & descriptor)
        {
            mRequest.subjectDescriptor = descriptor;
            return *this;
        }

        ConstructionArguments & SetReadFlags(const BitFlags<DataModel::ReadFlags> & flags)
        {
            mRequest.readFlags = flags;
            return *this;
        }

        ConstructionArguments & SetOperationFlags(const BitFlags<DataModel::OperationFlags> & flags)
        {
            mRequest.operationFlags = flags;
            return *this;
        }

        const DataModel::ReadAttributeRequest & Request() const { return mRequest; }

    private:
        DataModel::ReadAttributeRequest mRequest;
    };

    TestReadRequest(const ConstructionArguments & arguments) : mRequest(arguments.Request()) {}

    /// Start the encoding of a new element with the given data version associated to it.
    ///
    /// The input attribute encoding state will be attached to the returned value encoded (so that
    /// encoding for list elements is possible)
    ///
    std::unique_ptr<AttributeValueEncoder> StartEncoding(const EncodingParams & params = EncodingParams());

    /// Completes the encoding and finalizes the undelying AttributeReport.
    CHIP_ERROR FinishEncoding();

    const DataModel::ReadAttributeRequest & GetRequest() const { return mRequest; }
    const EncodedReportIBs& GetEncodedIBs() const { return mEncodedIBs; }

private:
    DataModel::ReadAttributeRequest mRequest;

    // encoded-used classes
    EncodedReportIBs mEncodedIBs;
    AttributeReportIBs::Builder mAttributeReportIBsBuilder;
};

} // namespace Testing
} // namespace app
} // namespace chip
