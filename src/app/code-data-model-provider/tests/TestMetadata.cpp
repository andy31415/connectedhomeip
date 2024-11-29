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
#include <optional>
#include <pw_unit_test/framework.h>

#include <access/Privilege.h>
#include <app/code-data-model-provider/Metadata.h>

using namespace chip::Access;
using namespace chip::app::Metadata;

TEST(TestMetadata, TestPrivilegeConversion)
{
    ASSERT_EQ(ReadPrivilege(AttributePrivilege::kNone), std::nullopt);
    ASSERT_EQ(WritePrivilege(AttributePrivilege::kNone), std::nullopt);

    ASSERT_EQ(ReadPrivilege(AttributePrivilege::kRead_View), std::make_optional(Privilege::kView));
    ASSERT_EQ(WritePrivilege(AttributePrivilege::kRead_View), std::nullopt);

    ASSERT_EQ(ReadPrivilege(AttributePrivilege::kWrite_Manage), std::nullopt);
    ASSERT_EQ(WritePrivilege(AttributePrivilege::kWrite_Manage), std::make_optional(Privilege::kManage));

    ASSERT_EQ(ReadPrivilege(AttributePrivilege::kRead_Administer | AttributePrivilege::kWrite_Operate),
              std::make_optional(Privilege::kAdminister));
    ASSERT_EQ(WritePrivilege(AttributePrivilege::kRead_Administer | AttributePrivilege::kWrite_Operate),
              std::make_optional(Privilege::kOperate));

    ASSERT_EQ(ReadPrivilege(AttributePrivilege::kRead_ProxyView | AttributePrivilege::kWrite_View),
              std::make_optional(Privilege::kProxyView));
    ASSERT_EQ(WritePrivilege(AttributePrivilege::kRead_ProxyView | AttributePrivilege::kWrite_View),
              std::make_optional(Privilege::kView));

    EXPECT_EQ(1, 1);
}
