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
#include <app/dynamic-data-model-provider/ClusterProvider.h>
#include <lib/core/CHIPError.h>

namespace chip {
namespace app {
namespace DynamicDataModel {

DataModel::ActionReturnStatus ClusterBase::ReadAttribute(const DataModel::InteractionModelContext & context,
                                                         const DataModel::ReadAttributeRequest & request,
                                                         AttributeValueEncoder & encoder)
{
    const AttributeDefinition * attribute = AttributesBegin();
    const AttributeDefinition * end       = AttributesEnd();

    for (; attribute != end; attribute++)
    {
        if (attribute->id == request.path.mAttributeId)
        {
            return (*attribute->readFunction)(context, request, encoder);
        }
    }

    return Protocols::InteractionModel::Status::UnsupportedRead;
}

DataModel::ActionReturnStatus ClusterBase::WriteAttribute(const DataModel::InteractionModelContext & context,
                                                          const DataModel::WriteAttributeRequest & request,
                                                          AttributeValueDecoder & decoder)
{
    // TODO: implement
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

std::optional<DataModel::ActionReturnStatus> ClusterBase::Invoke(const DataModel::InteractionModelContext & context,
                                                                 const DataModel::InvokeRequest & request,
                                                                 chip::TLV::TLVReader & input_arguments, CommandHandler * handler)
{
    // TODO: implement
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

} // namespace DynamicDataModel
} // namespace app
} // namespace chip
