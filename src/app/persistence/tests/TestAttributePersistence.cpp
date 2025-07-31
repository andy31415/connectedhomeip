/*
 *    Copyright (c) 2025 Project CHIP Authors
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
#include <pw_unit_test/framework.h>

#include <app/AttributeValueDecoder.h>
#include <app/ConcreteAttributePath.h>
#include <app/data-model-provider/tests/WriteTesting.h>
#include <app/persistence/AttributePersistence.h>
#include <app/persistence/DefaultAttributePersistenceProvider.h>
#include <lib/core/CHIPError.h>
#include <lib/core/StringBuilderAdapters.h>
#include <lib/support/DefaultStorageKeyAllocator.h>
#include <lib/support/Span.h>
#include <lib/support/TestPersistentStorageDelegate.h>

namespace {

using namespace chip;
using namespace chip::app;
using namespace chip::app::Testing;

TEST(TestAttributePersistence, TestLoadAndStoreNativeEndian)
{
    TestPersistentStorageDelegate storageDelegate;
    DefaultAttributePersistenceProvider ramProvider;
    ASSERT_EQ(ramProvider.Init(&storageDelegate), CHIP_NO_ERROR);

    AttributePersistence persistence(ramProvider);

    const ConcreteAttributePath path(1, 2, 3);
    const ConcreteAttributePath wrongPath(1, 2, 4);
    constexpr uint32_t kValueToStore = 42;
    constexpr uint32_t kOtherValue   = 99;

    // Store a fake value
    {
        const uint32_t value = kValueToStore;
        EXPECT_EQ(storageDelegate.SyncSetKeyValue(
                      DefaultStorageKeyAllocator::AttributeValue(path.mEndpointId, path.mClusterId, path.mAttributeId).KeyName(),
                      &value, sizeof(value)),
                  CHIP_NO_ERROR);
    }

    // Test loading a value
    {
        uint32_t valueRead = 0;

        ASSERT_TRUE(persistence.LoadNativeEndianValue(path, valueRead, kOtherValue));
        ASSERT_EQ(valueRead, kValueToStore);
    }

    // Test loading a non-existent value
    {
        uint32_t valueRead = 0;

        ASSERT_FALSE(persistence.LoadNativeEndianValue(wrongPath, valueRead, kOtherValue));
        ASSERT_EQ(valueRead, kOtherValue);
    }

    // Test loading a removed value
    {
        EXPECT_EQ(storageDelegate.SyncDeleteKeyValue(
                      DefaultStorageKeyAllocator::AttributeValue(path.mEndpointId, path.mClusterId, path.mAttributeId).KeyName()),
                  CHIP_NO_ERROR);

        uint32_t valueRead = 0;
        ASSERT_FALSE(persistence.LoadNativeEndianValue(path, valueRead, kOtherValue));
        ASSERT_EQ(valueRead, kOtherValue);
    }
}

TEST(TestAttributePersistence, TestNativeRawValueViaDecoder)
{
    TestPersistentStorageDelegate storageDelegate;
    DefaultAttributePersistenceProvider ramProvider;
    ASSERT_EQ(ramProvider.Init(&storageDelegate), CHIP_NO_ERROR);

    AttributePersistence persistence(ramProvider);

    const ConcreteAttributePath path(1, 2, 3);
    const ConcreteAttributePath wrongPath(1, 2, 4);
    constexpr uint32_t kValueToStore = 0x12345678;
    constexpr uint32_t kOtherValue   = 0x99887766;
    uint32_t valueRead               = 0;

    // Store a value using an encoder (these are a PAIN to create, so use data model provider helpers)
    {
        WriteOperation writeOp(path);
        AttributeValueDecoder decoder = writeOp.DecoderFor(kValueToStore);
        EXPECT_EQ(persistence.StoreNativeEndianValue(path, decoder, valueRead), CHIP_NO_ERROR);
        EXPECT_EQ(valueRead, kValueToStore);
    }

    {
        valueRead = 0;
        ASSERT_TRUE(persistence.LoadNativeEndianValue(path, valueRead, kOtherValue));
        ASSERT_EQ(valueRead, kValueToStore);
    }

    // Try to read non-compatible types (note that size-wise compatible types will work ... wrongly (like u32 and float))
    // this extra check is best-effort
    {
        uint16_t smallValue   = 0;
        const uint16_t kOther = 123u;

        ASSERT_FALSE(persistence.LoadNativeEndianValue(path, smallValue, kOther));
        ASSERT_EQ(smallValue, kOther);
    }
    {
        uint64_t largeValue   = 0;
        const uint64_t kOther = 0x1122334455667788ull;

        ASSERT_FALSE(persistence.LoadNativeEndianValue(path, largeValue, kOther));
        ASSERT_EQ(largeValue, kOther);
    }
}

} // namespace
