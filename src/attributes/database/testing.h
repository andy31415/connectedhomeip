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
#pragma once

#include <attributes/database/interface.h>

namespace chip {
namespace Attributes {

/// Scoped replace of the active database.
///
/// Used for a scoped replacement of the global database instance.
/// Generally used to scope a database change during execution of
/// a test case.
///
/// NOTE: This scoped replacement assumes that tests run in a single
///       thread. This is not full injection dependency.
class ScopedDatabase
{
public:
    TestScopedDatabase(Database * new_value) : mOldDatabase(SetDatabase(new_value)) {}
    ~TestScopedDatabase()
    {
        if (mOldDatbase != nullptr)
        {
            SetDatabase(mOldDatabase);
        }
    }

private:
    Database * mOldDatabase;
};

} // namespace Attributes
} // namespace chip
