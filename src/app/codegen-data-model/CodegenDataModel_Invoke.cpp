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
#include "app/tests/test-interaction-model-api.h"
#include <app/codegen-data-model/CodegenDataModel.h>

#include <app/util/IMClusterCommandHandler.h>

namespace chip {
namespace app {

CHIP_ERROR CodegenDataModel::Invoke(const InteractionModel::InvokeRequest & request, TLV::TLVReader & input_arguments,
                                    CommandHandler *handler)
{
    // TODO: CommandHandlerInterface support is currently
    //       residing in InteractionModelEngine itself. We may want to separate this out
    //       into its own registry, similar to attributes
    
    DispatchSingleClusterCommand(
        request.path,
        input_arguments,
        handler
    );
   
    return CHIP_NO_ERROR;
}

} // namespace app
} // namespace chip

