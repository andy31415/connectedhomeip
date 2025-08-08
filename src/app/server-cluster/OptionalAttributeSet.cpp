/*
 *    Copyright (c) 2025 Project CHIP Authors
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
#include <app/server-cluster/OptionalAttributeSet.h>

namespace chip {
namespace app {

CHIP_ERROR AttributeSet::AppendEnabled(ReadOnlyBufferBuilder<DataModel::AttributeEntry> & builder) const
{
    ReturnErrorOnFailure(builder.EnsureAppendCapacity(mSupportedAttributes.size()));

    unsigned idx = 0;
    for (const auto entry : mSupportedAttributes)
    {
        if ((mSetBits & (1 << idx)) != 0)
        {
            ReturnErrorOnFailure(builder.Append(entry));
        }
        idx++;
    }

    return CHIP_NO_ERROR;
}

} // namespace app
} // namespace chip
