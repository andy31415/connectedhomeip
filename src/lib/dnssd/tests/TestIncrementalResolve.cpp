/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#include <lib/dnssd/IncrementalResolve.h>

#include <string.h>

#include <lib/support/UnitTestRegistration.h>

#include <nlunit-test.h>

using namespace chip;
using namespace chip::Dnssd;

namespace {

void TestCreation(nlTestSuite * inSuite, void * inContext)
{
    IncrementalResolver resolver;

    NL_TEST_ASSERT(inSuite, !resolver.IsActive());
    NL_TEST_ASSERT(inSuite, !resolver.IsActiveCommissionParse());
    NL_TEST_ASSERT(inSuite, !resolver.IsActiveOperationalParse());
}

const nlTest sTests[] = {
    NL_TEST_DEF("Creation", TestCreation), //
    NL_TEST_SENTINEL()                     //
};

} // namespace

int TestChipDnsSdIncrementalResolve(void)
{
    nlTestSuite theSuite = { "IncrementalResolve", &sTests[0], nullptr, nullptr };
    nlTestRunner(&theSuite, nullptr);
    return nlTestRunnerStats(&theSuite);
}

CHIP_REGISTER_TEST_SUITE(TestChipDnsSdIncrementalResolve)
