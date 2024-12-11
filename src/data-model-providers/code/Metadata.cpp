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
#include <data-model-providers/code/Metadata.h>

namespace chip {
namespace app {
namespace Metadata {

std::optional<Access::Privilege> ReadPrivilege(std::underlying_type_t<AttributePrivilege> value)
{
    switch (value & 0x07)
    {
    case AttributePrivilege::kRead_View:
        return Access::Privilege::kView;
    case AttributePrivilege::kRead_ProxyView:
        return Access::Privilege::kProxyView;
    case AttributePrivilege::kRead_Operate:
        return Access::Privilege::kOperate;
    case AttributePrivilege::kRead_Manage:
        return Access::Privilege::kManage;
    case AttributePrivilege::kRead_Administer:
        return Access::Privilege::kAdminister;
    default:
        return std::nullopt;
    }
}

std::optional<Access::Privilege> WritePrivilege(std::underlying_type_t<AttributePrivilege> value)
{
    // ugly bit-shift to make the code smaller and re-use a function
    return ReadPrivilege(value >> 3);
}

} // namespace Metadata
} // namespace app
} // namespace chip
