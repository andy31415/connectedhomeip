/*
 *
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

#include "access/Privilege.h"
#include "app-common/zap-generated/cluster-enums.h"
#include "app-common/zap-generated/ids/Attributes.h"
#include <app/dynamic-data-model-provider/ClusterProvider.h>
#include <lib/core/StringBuilderAdapters.h>

#include <app-common/zap-generated/cluster-objects.h>

#include <pw_unit_test/framework.h>

namespace {

using namespace chip::app;
using namespace chip::app::DynamicDataModel;

class TestCluster : public Cluster<2 /* kAttributeCount */>
{
public:
    using Base = Cluster<2>;

    constexpr TestCluster() :
        Base({
            AttributeDefinition(Clusters::UnitTesting::Attributes::Boolean::Id),
            AttributeDefinition(Clusters::UnitTesting::Attributes::Bitmap8::Id)
                .SetWritePrivilege(chip::Access::Privilege::kAdminister),

            // WHAT I want:
            //   .SetReadFunction(Base::ReadVia(GetBitmap8Value))
            //   .SetWriteFunction(Base::WriteVia(SetBitmap8Value))
            //
            // Issues:
            //   - input data to function is DYNAMIC so lambda bridge does NOT work
            //   - May need something else here, to be trivial. Modeled AFTER lambda bridge
        })
    {}

private:
    bool GetBoolValue();

    chip::BitMask<Clusters::UnitTesting::Bitmap8MaskMap> GetBitmap8Value();
    CHIP_ERROR SetBitmap8Value(const chip::BitMask<Clusters::UnitTesting::Bitmap8MaskMap> &);
};

} // namespace

TEST(TestClusterProvider, BasicTest)
{
    ASSERT_EQ(1, 1);
}
