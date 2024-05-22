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
#include <app/codegen-interaction-model/CodegenDataModel.h>

#include <access/AccessControl.h>
#include <app/AttributeAccessInterface.h>
#include <app/AttributeAccessInterfaceRegistry.h>
#include <app/RequiredPrivilege.h>
#include <app/codegen-interaction-model/EmberMetadata.h>

namespace chip {
namespace app {
namespace {

/// Attempts to write via an attribute access interface (AAI)
///
/// If it returns a CHIP_ERROR, then this is a FINAL result (i.e. either failure or success):
///    - in particular, CHIP_ERROR_ACCESS_DENIED will be used for UnsupportedRead AII returns
///
/// If it returns std::nullopt, then there is no AAI to handle the given path
/// and processing should figure out the value otherwise (generally from other ember data)
std::optional<CHIP_ERROR> TryWriteViaAccessInterface(const ConcreteAttributePath & path, AttributeAccessInterface * aai,
                                                     AttributeValueDecoder & decoder)
{

    // Processing can happen only if an attribute access interface actually exists..
    if (aai == nullptr)
    {
        return std::nullopt;
    }

    CHIP_ERROR err = aai->Write(path, decoder);

    // explict translate UnsupportedRead to Access denied. This is to allow callers to determine a
    // translation for this: usually wildcard subscriptions MAY just ignore these where as direct reads
    // MUST translate them to UnsupportedAccess
    ReturnErrorCodeIf(err == CHIP_IM_GLOBAL_STATUS(UnsupportedWrite), CHIP_ERROR_ACCESS_DENIED);

    if (err != CHIP_NO_ERROR)
    {
        return std::make_optional(err);
    }

    // If the decoder tried to decode, then a value should have been read for processing.
    //   - if decode, assueme DONE (i.e. FINAL CHIP_NO_ERROR)
    //   - if no encode, say that processing must continue
    return decoder.TriedDecode() ? std::make_optional(CHIP_NO_ERROR) : std::nullopt;
}

} // namespace

CHIP_ERROR CodegenDataModel::WriteAttribute(const InteractionModel::WriteAttributeRequest & request,
                                            AttributeValueDecoder & decoder)
{
    ChipLogDetail(DataManagement,
                  "Reading attribute: Cluster=" ChipLogFormatMEI " Endpoint=%x AttributeId=" ChipLogFormatMEI " (expanded=%d)",
                  ChipLogValueMEI(request.path.mClusterId), request.path.mEndpointId, ChipLogValueMEI(request.path.mAttributeId),
                  request.path.mExpanded);

    // ACL check for non-internal requests
    if (!request.operationFlags.Has(InteractionModel::OperationFlags::kInternal))
    {
        ReturnErrorCodeIf(!request.subjectDescriptor.has_value(), CHIP_ERROR_INVALID_ARGUMENT);

        Access::RequestPath requestPath{ .cluster = request.path.mClusterId, .endpoint = request.path.mEndpointId };
        ReturnErrorOnFailure(Access::GetAccessControl().Check(*request.subjectDescriptor, requestPath,
                                                              RequiredPrivilege::ForWriteAttribute(request.path)));
    }

    auto metadata = Ember::FindAttributeMetadata(request.path);

    // Explicit failure in finding a suitable metadata
    if (const CHIP_ERROR * err = std::get_if<CHIP_ERROR>(&metadata))
    {
        VerifyOrDie(*err != CHIP_NO_ERROR);
        return *err;
    }

    // All the global attributes that we do not have metadata for are
    // read-only (i.e. cannot write atribute_list/event_list/accepted_cmds/generated_cmds)
    //
    // so if no metadata available, we wil lreturn an error
    const EmberAfAttributeMetadata ** meta = std::get_if<const EmberAfAttributeMetadata *>(&metadata);
    if (meta == nullptr)
    {
        return CHIP_IM_GLOBAL_STATUS(UnsupportedWrite);
    }

    if ((*meta)->IsReadOnly() && !request.operationFlags.Has(InteractionModel::OperationFlags::kInternal))
    {
        // Internal is allowed to try to bypass read-only updates, however otherwise we deny read-only
        // updates
        return CHIP_IM_GLOBAL_STATUS(UnsupportedWrite);
    }

    if ((*meta)->MustUseTimedWrite() && !request.writeFlags.Has(InteractionModel::WriteFlags::kTimed))
    {
        return CHIP_IM_GLOBAL_STATUS(NeedsTimedInteraction);
    }

    if (request.path.mDataVersion.HasValue())
    {
        std::optional<InteractionModel::ClusterInfo> clusterInfo = GetClusterInfo(request.path);
        if (!clusterInfo.has_value())
        {
            ChipLogError(DataManagement, "Unable to get cluster info for Endpoint %x, Cluster " ChipLogFormatMEI,
                         request.path.mEndpointId, ChipLogValueMEI(request.path.mClusterId));
            return CHIP_IM_GLOBAL_STATUS(DataVersionMismatch);
        }

        if (request.path.mDataVersion.Value() != clusterInfo->dataVersion)
        {
            ChipLogError(DataManagement, "Write Version mismatch for Endpoint %x, Cluster " ChipLogFormatMEI,
                         request.path.mEndpointId, ChipLogValueMEI(request.path.mClusterId));
            return CHIP_IM_GLOBAL_STATUS(DataVersionMismatch);
        }
    }

    std::optional<CHIP_ERROR> aai_result = TryWriteViaAccessInteraface(
        request.path, GetAttributeAccessOverride(request.path.mEndpointId, request.path.mClusterId), decoder);
    ReturnErrorCodeIf(aai_result.has_value(), *aai_result);

    // TODO:
    //   - ember write

    return CHIP_ERROR_NOT_IMPLEMENTED;
#if 0
    ////////// EMBER (remaining only)///////

    CHIP_ERROR preparationError = CHIP_NO_ERROR;
    uint16_t dataLen            = 0;
    if ((preparationError = prepareWriteData(attributeMetadata, aReader, dataLen)) != CHIP_NO_ERROR)
    {
        ChipLogDetail(Zcl, "Failed to prepare data to write: %" CHIP_ERROR_FORMAT, preparationError.Format());
        return apWriteHandler->AddStatus(aPath, Protocols::InteractionModel::Status::InvalidValue);
    }

    if (dataLen > attributeMetadata->size)
    {
        ChipLogDetail(Zcl, "Data to write exceedes the attribute size claimed.");
        return apWriteHandler->AddStatus(aPath, Protocols::InteractionModel::Status::InvalidValue);
    }

    auto status = emAfWriteAttributeExternal(aPath.mEndpointId, aPath.mClusterId, aPath.mAttributeId,
                                             gEmberAttributeIOBufferSpan.data(), attributeMetadata->attributeType);
    return apWriteHandler->AddStatus(aPath, status);
#endif
}

} // namespace app
} // namespace chip
