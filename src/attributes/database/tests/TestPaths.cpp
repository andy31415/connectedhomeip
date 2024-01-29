/*
 *    Copyright (c) 2024 Project CHIP Authors
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
#include <attributes/database/paths.h>
#include <lib/support/UnitTestRegistration.h>

#include <nlunit-test.h>

using namespace chip::Attributes;

namespace {

void TestIdPathEquality(nlTestSuite * inSuite, void * inContext)
{

    Cluster::Path c1(Endpoint::Id(1), Cluster::Id(2));
    Cluster::Path c2(Endpoint::Id(1), Cluster::Id(2));
    Cluster::Path c3(Endpoint::Id(1), Cluster::Id(3));

    NL_TEST_ASSERT(inSuite, c1 == c2);
    NL_TEST_ASSERT(inSuite, c1 != c3);
    NL_TEST_ASSERT(inSuite, c2 != c3);

    Attribute::Path a1(c1, Attribute::Id(100));
    Attribute::Path a2(Endpoint::Id(1), Cluster::Id(2), Attribute::Id(100));
    Attribute::Path a3(c3, Attribute::Id(100));
    Attribute::Path a4(Endpoint::Id(1), Cluster::Id(3), Attribute::Id(100));

    NL_TEST_ASSERT(inSuite, a1 == a2);
    NL_TEST_ASSERT(inSuite, a2 != a3);
    NL_TEST_ASSERT(inSuite, a3 == a4);
}

void TestIndexPathEquality(nlTestSuite * inSuite, void * inContext)
{

    Cluster::IndexPath c1(Endpoint::Index(1), Cluster::Index(2));
    Cluster::IndexPath c2(Endpoint::Index(1), Cluster::Index(2));
    Cluster::IndexPath c3(Endpoint::Index(1), Cluster::Index(3));

    NL_TEST_ASSERT(inSuite, c1 == c2);
    NL_TEST_ASSERT(inSuite, c1 != c3);
    NL_TEST_ASSERT(inSuite, c2 != c3);

    Attribute::IndexPath a1(c1, Attribute::Index(100));
    Attribute::IndexPath a2(Endpoint::Index(1), Cluster::Index(2), Attribute::Index(100));
    Attribute::IndexPath a3(c3, Attribute::Index(100));
    Attribute::IndexPath a4(Endpoint::Index(1), Cluster::Index(3), Attribute::Index(100));

    NL_TEST_ASSERT(inSuite, a1 == a2);
    NL_TEST_ASSERT(inSuite, a2 != a3);
    NL_TEST_ASSERT(inSuite, a3 == a4);
}

const nlTest sTests[] = {
    NL_TEST_DEF("TestIdPathEquality", TestIdPathEquality),       //
    NL_TEST_DEF("TestIndexPathEquality", TestIndexPathEquality), //
    NL_TEST_SENTINEL()                                           //
};

} // namespace

int TestAttributesDatabase()
{
    nlTestSuite theSuite = { "Attributes database tests", &sTests[0], nullptr, nullptr };

    // Run test suite against one context.
    nlTestRunner(&theSuite, nullptr);
    return nlTestRunnerStats(&theSuite);
}

CHIP_REGISTER_TEST_SUITE(TestAttributesDatabase)
