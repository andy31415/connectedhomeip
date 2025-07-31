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
#include <app/persistence/PascalString.h>
#include <lib/core/CHIPError.h>
#include <lib/core/StringBuilderAdapters.h>
#include <lib/support/DefaultStorageKeyAllocator.h>
#include <lib/support/Span.h>
#include <lib/support/TestPersistentStorageDelegate.h>

namespace {

using namespace chip;
using namespace chip::app;
using namespace chip::app::Testing;
using namespace chip::app::Storage;
using namespace chip::app::Persistence;

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

        ASSERT_TRUE(persistence.Load<NativeEndian<uint32_t>>(path, valueRead, kOtherValue));
        ASSERT_EQ(valueRead, kValueToStore);
    }

    // Test loading a non-existent value
    {
        uint32_t valueRead = 0;

        ASSERT_FALSE(persistence.Load<NativeEndian<uint32_t>>(wrongPath, valueRead, kOtherValue));
        ASSERT_EQ(valueRead, kOtherValue);
    }

    // Test loading a removed value
    {
        EXPECT_EQ(storageDelegate.SyncDeleteKeyValue(
                      DefaultStorageKeyAllocator::AttributeValue(path.mEndpointId, path.mClusterId, path.mAttributeId).KeyName()),
                  CHIP_NO_ERROR);

        uint32_t valueRead = 0;
        ASSERT_FALSE(persistence.Load<NativeEndian<uint32_t>>(path, valueRead, kOtherValue));
        ASSERT_EQ(valueRead, kOtherValue);
    }
}

TEST(TestAttributePersistence, TestLoadAndStoreString)
{
    TestPersistentStorageDelegate storageDelegate;
    DefaultAttributePersistenceProvider ramProvider;
    ASSERT_EQ(ramProvider.Init(&storageDelegate), CHIP_NO_ERROR);

    AttributePersistence persistence(ramProvider);
    const ConcreteAttributePath path(1, 2, 3);

    // Store a fake value
    {
        const uint8_t buffer[] = { 5, 'h', 'e', 'l', 'l', 'o' };
        EXPECT_EQ(storageDelegate.SyncSetKeyValue(
                      DefaultStorageKeyAllocator::AttributeValue(path.mEndpointId, path.mClusterId, path.mAttributeId).KeyName(),
                      buffer, sizeof(buffer)),
                  CHIP_NO_ERROR);
    }

    // Test loading a value
    {
        char bufferRead[16];
        ShortPascalString stringRead(bufferRead);

        ASSERT_TRUE(persistence.Load<String<ShortPascalString>>(path, stringRead, ""_span));
        ASSERT_TRUE(stringRead.Content().data_equal("hello"_span));
    }

    // Test loading a non-existent value
    {
        const ConcreteAttributePath wrongPath(1, 2, 4);
        char bufferRead[16];
        ShortPascalString stringRead(bufferRead);

        ASSERT_FALSE(persistence.Load<String<ShortPascalString>>(wrongPath, stringRead, "default"_span));
        ASSERT_TRUE(stringRead.Content().data_equal("default"_span));
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
        EXPECT_EQ(persistence.Store<NativeEndian<uint32_t>>(path, decoder, valueRead), CHIP_NO_ERROR);
        EXPECT_EQ(valueRead, kValueToStore);
    }

    {
        valueRead = 0;
        ASSERT_TRUE(persistence.Load<NativeEndian<uint32_t>>(path, valueRead, kOtherValue));
        ASSERT_EQ(valueRead, kValueToStore);
    }

    // Try to read non-compatible types (note that size-wise compatible types will work ... wrongly (like u32 and float))
    // this extra check is best-effort
    {
        uint16_t smallValue   = 0;
        const uint16_t kOther = 123u;

        ASSERT_FALSE(persistence.Load<NativeEndian<uint16_t>>(path, smallValue, kOther));
        ASSERT_EQ(smallValue, kOther);
    }
    {
        uint64_t largeValue   = 0;
        const uint64_t kOther = 0x1122334455667788ull;

        ASSERT_FALSE(persistence.Load<Persistence::NativeEndian<uint64_t>>(path, largeValue, kOther));
        ASSERT_EQ(largeValue, kOther);
    }
}

TEST(TestAttributePersistence, TestStringViaDecoder)
{
    TestPersistentStorageDelegate storageDelegate;
    DefaultAttributePersistenceProvider ramProvider;
    ASSERT_EQ(ramProvider.Init(&storageDelegate), CHIP_NO_ERROR);

    AttributePersistence persistence(ramProvider);

    const ConcreteAttributePath path(1, 2, 3);

    // Store a value using an encoder (these are a PAIN to create, so use data model provider helpers)
    {
        WriteOperation writeOp(path);
        AttributeValueDecoder decoder = writeOp.DecoderFor("hello world"_span);

        char buffer[32];
        EXPECT_EQ(persistence.StorePascalString(path, decoder, buffer), CHIP_NO_ERROR);

        ShortPascalString stringStored(buffer);
        EXPECT_TRUE(stringStored.Content().data_equal("hello world"_span));
    }

    {
        char bufferRead[32];
        ShortPascalString stringRead(bufferRead);

        ASSERT_TRUE(persistence.Load<String<ShortPascalString>>(path, stringRead, ""_span));
        ASSERT_TRUE(stringRead.Content().data_equal("hello world"_span));
    }
}

TEST(TestAttributePersistence, TestByteStringViaDecoder)
{
    TestPersistentStorageDelegate storageDelegate;
    DefaultAttributePersistenceProvider ramProvider;
    ASSERT_EQ(ramProvider.Init(&storageDelegate), CHIP_NO_ERROR);

    AttributePersistence persistence(ramProvider);

    const ConcreteAttributePath path(1, 2, 3);
    const uint8_t binary_data[] = { 1, 2, 3, 4, 0, 255, 128 };

    // Store a value using an encoder (these are a PAIN to create, so use data model provider helpers)
    {
        WriteOperation writeOp(path);
        AttributeValueDecoder decoder = writeOp.DecoderFor(ByteSpan(binary_data));

        uint8_t buffer[32];
        EXPECT_EQ(persistence.StorePascalString(path, decoder, buffer), CHIP_NO_ERROR);

        ShortPascalBytes bytesStored(buffer);
        EXPECT_TRUE(bytesStored.Content().data_equal(ByteSpan(binary_data)));
    }

    {
        uint8_t bufferRead[32];
        ShortPascalBytes bytesRead(bufferRead);

        ASSERT_TRUE(persistence.Load<String<ShortPascalBytes>>(path, bytesRead, ByteSpan()));
        ASSERT_TRUE(bytesRead.Content().data_equal(ByteSpan(binary_data)));
    }
}

TEST(TestAttributePersistence, TestByteStringLoadWithDefaults)
{
    TestPersistentStorageDelegate storageDelegate;
    DefaultAttributePersistenceProvider ramProvider;
    ASSERT_EQ(ramProvider.Init(&storageDelegate), CHIP_NO_ERROR);

    AttributePersistence persistence(ramProvider);

    const ConcreteAttributePath path(1, 2, 3);
    const uint8_t default_binary_data[] = { 10, 20, 30, 40 };

    uint8_t bufferRead[32];
    ShortPascalBytes bytesRead(bufferRead);

    ASSERT_FALSE(persistence.Load<String<ShortPascalBytes>>(path, bytesRead, ByteSpan(default_binary_data)));
    ASSERT_TRUE(bytesRead.Content().data_equal(ByteSpan(default_binary_data)));
}

TEST(TestAttributePersistence, TestCharStringLoadWithDefaults)
{
    TestPersistentStorageDelegate storageDelegate;
    DefaultAttributePersistenceProvider ramProvider;
    ASSERT_EQ(ramProvider.Init(&storageDelegate), CHIP_NO_ERROR);

    AttributePersistence persistence(ramProvider);

    const ConcreteAttributePath path(1, 2, 3);

    char bufferRead[32];
    ShortPascalString stringRead(bufferRead);

    ASSERT_FALSE(persistence.Load<String<ShortPascalString>>(path, stringRead, "default value"_span));
    ASSERT_TRUE(stringRead.Content().data_equal("default value"_span));
}

TEST(TestAttributePersistence, TestStoreNullByteString)
{
    TestPersistentStorageDelegate storageDelegate;
    DefaultAttributePersistenceProvider ramProvider;
    ASSERT_EQ(ramProvider.Init(&storageDelegate), CHIP_NO_ERROR);

    AttributePersistence persistence(ramProvider);

    const ConcreteAttributePath path(1, 2, 3);
    const ConcreteAttributePath path2(1, 2, 4);

    // Store a value using an encoder (these are a PAIN to create, so use data model provider helpers)
    {
        WriteOperation writeOp(path);
        AttributeValueDecoder decoder = writeOp.DecoderFor(DataModel::Nullable<ByteSpan>());

        uint8_t buffer[32];
        EXPECT_EQ(persistence.StorePascalString(path, decoder, buffer), CHIP_NO_ERROR);

        ShortPascalBytes bytesStored(buffer);
        EXPECT_TRUE(bytesStored.IsNull());
    }

    // reading back on an invalid path will fail the load and a default value will be returned
    {
        uint8_t bufferRead[32];
        ShortPascalBytes bytesRead(bufferRead);

        const uint8_t default_binary_data[] = { 1, 2, 3 };

        ASSERT_FALSE(persistence.Load<String<ShortPascalBytes>>(path2, bytesRead, ByteSpan(default_binary_data)));
        ASSERT_TRUE(bytesRead.Content().data_equal(ByteSpan(default_binary_data)));
    }
}

TEST(TestAttributePersistence, TestStoreNullCharString)
{
    TestPersistentStorageDelegate storageDelegate;
    DefaultAttributePersistenceProvider ramProvider;
    ASSERT_EQ(ramProvider.Init(&storageDelegate), CHIP_NO_ERROR);

    AttributePersistence persistence(ramProvider);

    const ConcreteAttributePath path(1, 2, 3);
    const ConcreteAttributePath path2(1, 2, 4);

    // Store a value using an encoder (these are a PAIN to create, so use data model provider helpers)
    {
        WriteOperation writeOp(path);
        AttributeValueDecoder decoder = writeOp.DecoderFor(DataModel::Nullable<CharSpan>());

        char buffer[32];
        EXPECT_EQ(persistence.StorePascalString(path, decoder, buffer), CHIP_NO_ERROR);

        ShortPascalString stringStored(buffer);
        EXPECT_TRUE(stringStored.IsNull());
    }

    // reading back on an invalid path will fail the load and a default value will be returned
    {
        char bufferRead[32];
        ShortPascalString stringRead(bufferRead);

        ASSERT_FALSE(persistence.Load<String<ShortPascalString>>(path2, stringRead, "default value"_span));
        ASSERT_TRUE(stringRead.Content().data_equal("default value"_span));
    }
}

TEST(TestAttributePersistence, TestLoadInvalidPascalString)
{
    TestPersistentStorageDelegate storageDelegate;
    DefaultAttributePersistenceProvider ramProvider;
    ASSERT_EQ(ramProvider.Init(&storageDelegate), CHIP_NO_ERROR);

    AttributePersistence persistence(ramProvider);
    const ConcreteAttributePath path(1, 2, 3);

    {
        // valid pascal string for "hello" that requires 6 bytes to store (1 for length)
        uint8_t buffer[] = { 5, 'h', 'e', 'l', 'l', 'o' };
        EXPECT_EQ(storageDelegate.SyncSetKeyValue(
                      DefaultStorageKeyAllocator::AttributeValue(path.mEndpointId, path.mClusterId, path.mAttributeId).KeyName(),
                      buffer, sizeof(buffer)),
                  CHIP_NO_ERROR);
    }

    // Test loading with too short of a buffer
    {
        char bufferRead[5]; // need 6 bytes here...
        ShortPascalString stringRead(bufferRead);

        ASSERT_FALSE(persistence.Load<String<ShortPascalString>>(path, stringRead, "def"_span));
        ASSERT_TRUE(stringRead.Content().data_equal("def"_span));
    }

    // Test loading with too short of a buffer and too long of a default:
    //  - string load fails (insufficient buffer)
    //  - default load fails because default does not fit either
    //  - this moves the data to be null
    {
        char bufferRead[5]; // need 6 bytes here...
        ShortPascalString stringRead(bufferRead);

        ASSERT_FALSE(persistence.Load<String<ShortPascalString>>(path, stringRead, "default"_span));

        // default could not be set (too long)
        ASSERT_TRUE(stringRead.IsNull());
    }
}

TEST(TestAttributePersistence, TestInvalidPascalLengthStored)
{
    TestPersistentStorageDelegate storageDelegate;
    DefaultAttributePersistenceProvider ramProvider;
    ASSERT_EQ(ramProvider.Init(&storageDelegate), CHIP_NO_ERROR);

    AttributePersistence persistence(ramProvider);
    const ConcreteAttributePath path(1, 2, 3);

    // This string is invalid as stored
    {
        uint8_t buffer[] = { 10, 'h', 'e', 'l', 'l', 'o' }; // length 10, but only 5 chars
        EXPECT_EQ(storageDelegate.SyncSetKeyValue(
                      DefaultStorageKeyAllocator::AttributeValue(path.mEndpointId, path.mClusterId, path.mAttributeId).KeyName(),
                      buffer, sizeof(buffer)),
                  CHIP_NO_ERROR);
    }

    // Load into a buffer that COULD contain the string, but
    // stored string is invalid
    {
        char bufferRead[16];
        ShortPascalString stringRead(bufferRead);

        ASSERT_FALSE(persistence.Load<String<ShortPascalString>>(path, stringRead, "default"_span));
        ASSERT_TRUE(stringRead.Content().data_equal("default"_span));
    }
}

} // namespace
