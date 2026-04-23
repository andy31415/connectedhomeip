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

#include <data-model-providers/codegen/EmberAttributeDecoder.h>
#include <data-model-providers/codegen/EmberAttributeDataBuffer.h>

#include <app/MessageDef/ReportDataMessage.h>
#include <app/AttributeReportBuilder.h>
#include <app-common/zap-generated/attribute-type.h>
#include <app/util/attribute-metadata.h>

namespace chip {
namespace app {

CHIP_ERROR DecodeAttributeToEmberBuffer(const AttributeDecoderParams & params, MutableByteSpan & outBuffer)
{
    // TODO: Sizing of gEmberAttributeIOBufferSpan needs to be updated to handle largest attribute + TLV overhead.
    // This will be done in a follow-up task.
    
    // 1. Setup TLV Writer and Builder using outBuffer as scratch space
    TLV::TLVWriter writer;
    writer.Init(outBuffer.data(), outBuffer.size());
    
    TLV::TLVType outerStructureType;
    ReturnErrorOnFailure(writer.StartContainer(TLV::AnonymousTag(), TLV::kTLVType_Structure, outerStructureType));
    
    app::AttributeReportIBs::Builder builder;
    ReturnErrorOnFailure(builder.Init(&writer, to_underlying(app::ReportDataMessage::Tag::kAttributeReportIBs)));
    
    // 2. Create AttributeValueEncoder
    Access::SubjectDescriptor subjectDescriptor; // Default subject descriptor for internal use
    
    app::AttributeValueEncoder encoder(builder, subjectDescriptor, params.path, 0 /* data version */);
    
    // 3. Read from cluster
    app::DataModel::ReadAttributeRequest request(params.path, subjectDescriptor);
    auto status = params.cluster.ReadAttribute(request, encoder);
    VerifyOrReturnError(status.IsSuccess(), status.GetUnderlyingError());
    
    // 4. Finalize encoding
    builder.EndOfContainer();
    ReturnErrorOnFailure(writer.EndContainer(outerStructureType));
    ReturnErrorOnFailure(writer.Finalize());
    
    // 5. Extract data from TLV
    TLV::TLVReader reader;
    reader.Init(outBuffer.data(), writer.GetLengthWritten());
    
    // Enter structure
    ReturnErrorOnFailure(reader.Next(TLV::kTLVType_Structure, TLV::AnonymousTag()));
    TLV::TLVType outer1;
    ReturnErrorOnFailure(reader.EnterContainer(outer1));
    
    // Enter Array
    ReturnErrorOnFailure(reader.Next(TLV::kTLVType_Array, TLV::ContextTag(to_underlying(app::ReportDataMessage::Tag::kAttributeReportIBs))));
    TLV::TLVType outer2;
    ReturnErrorOnFailure(reader.EnterContainer(outer2));
    
    // Next item should be AttributeReportIB
    ReturnErrorOnFailure(reader.Next(TLV::kTLVType_Structure, TLV::AnonymousTag()));
    
    app::AttributeReportIB::Parser attributeReportParser;
    ReturnErrorOnFailure(attributeReportParser.Init(reader));
    
    app::AttributeDataIB::Parser dataParser;
    ReturnErrorOnFailure(attributeReportParser.GetAttributeData(&dataParser));
    
    TLV::TLVReader attributeDataReader;
    ReturnErrorOnFailure(dataParser.GetData(&attributeDataReader));
    
    // 6. Use EmberAttributeDataBuffer to decode
    EmberAfAttributeMetadata dummyMeta = {
        .defaultValue  = EmberAfDefaultOrMinMaxAttributeValue(static_cast<uint8_t *>(nullptr)),
        .attributeId   = params.path.mAttributeId,
        .size          = params.emberSize,
        .attributeType = params.emberType,
        .mask          = 0, // Assume non-nullable for now
    };
    
    // We use outBuffer again as the target for decoding.
    // For Boolean, it is safe because the value is read before any large write happens (only 1 byte write).
    MutableByteSpan targetBuffer(outBuffer.data(), outBuffer.size());
    Ember::EmberAttributeDataBuffer emberBuffer(&dummyMeta, targetBuffer);
    
    if (params.emberType == ZCL_BOOLEAN_ATTRIBUTE_TYPE)
    {
        CHIP_ERROR err = emberBuffer.Decode(attributeDataReader);
        if (err == CHIP_NO_ERROR)
        {
            // Update outBuffer size to reflect the actual data written
            outBuffer.reduce_size(targetBuffer.size());
        }
        return err;
    }
    
    // TODO: Support more types
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

} // namespace app
} // namespace chip
