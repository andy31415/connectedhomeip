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
#pragma once

#include "lib/core/CHIPError.h"
#include <app/AttributeValueDecoder.h>
#include <app/ConcreteAttributePath.h>
#include <app/data-model-provider/ActionReturnStatus.h>
#include <app/persistence/AttributePersistenceProvider.h>
#include <app/persistence/PascalString.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/Span.h>

#include <type_traits>

namespace chip::app {

namespace Persistence {

/// Handles integer values encode/decode:
///   - values are stored in NATIVE endianness
template <typename T, typename std::enable_if<std::is_arithmetic_v<T>>::type * = nullptr>
struct NativeEndian
{
    static CHIP_ERROR Decode(AttributeValueDecoder & decoder, T & value) { return decoder.Decode(value); }
    static ByteSpan ValueToStore(const T & value) { return { reinterpret_cast<const uint8_t *>(&value), sizeof(value) }; }
    static MutableByteSpan StartLoad(T & value) { return { reinterpret_cast<uint8_t *>(&value), sizeof(value) }; }
    static bool FinalizeLoad(T & value, ByteSpan actualDataLoaded) { return actualDataLoaded.size() == sizeof(T); }
    static void LoadDefault(T & value, const T & default_value) { value = default_value; }
};

/// Handles reading/writing of pascal strings
template <typename PASCAL_TYPE>
struct String
{
    static CHIP_ERROR Decode(AttributeValueDecoder & decoder, PASCAL_TYPE & value)
    {
        DataModel::Nullable<typename PASCAL_TYPE::ValueType> valueSpan;
        ReturnErrorOnFailure(decoder.Decode(valueSpan));
        if (valueSpan.IsNull())
        {
            value.SetNull();
        }
        else
        {
            VerifyOrReturnError(value.SetValue(valueSpan.Value()), CHIP_IM_GLOBAL_STATUS(ConstraintError));
        }
        return CHIP_NO_ERROR;
    }
    static ByteSpan ValueToStore(const PASCAL_TYPE & value) { return value.ContentWithLenPrefix(); }
    static MutableByteSpan StartLoad(PASCAL_TYPE & value) { return value.RawFullBuffer(); }
    static bool FinalizeLoad(PASCAL_TYPE & value, ByteSpan actualDataLoaded)
    {
        // Byte span could be of any type. Reinterpret it
        return PASCAL_TYPE::IsValid(
            { reinterpret_cast<typename PASCAL_TYPE::ValueType::pointer>(actualDataLoaded.data()), actualDataLoaded.size() });
    }
    static void LoadDefault(PASCAL_TYPE & value, const typename PASCAL_TYPE::ValueType & default_value)
    {
        if (!value.SetValue(default_value))
        {
            value.SetNull();
        }
    }
};

}; // namespace Persistence

/// Provides functionality for handling attribute persistence via
/// an AttributePersistenceProvider.
///
/// AttributePersistenceProvider works with raw bytes, however attributes
/// have known (strong) types and their load/decode logic is often
/// similar and reusable. This class implements the logic of handling
/// such attributes, so that it can be reused across cluster implementations.
///
/// Usage example:
///
///   uint32_t mValue;
///   ConcreteAttributePath path;
///   AttributePersistence persistence(persistenceProvider);
///
///
///   persistence.Store<NativeEndian<uint32_t>>(path, mValue);
///   persistence.Load<NativeEndian<uint32_t>>(path, mValue, 1234);
///
///
class AttributePersistence
{
public:
    AttributePersistence(AttributePersistenceProvider & provider) : mProvider(provider) {}

    /// Store the given value in persistent storage.
    ///
    /// Generally just converts the value into data bytes and stores them as-is
    template <typename ENCODER, typename T>
    CHIP_ERROR Store(const ConcreteAttributePath & path, AttributeValueDecoder & decoder, T & value)
    {
        ReturnErrorOnFailure(ENCODER::Decode(decoder, value));
        return mProvider.WriteValue(path, ENCODER::ValueToStore(value));
    }

    /// Load a value from persistent storage.
    ///
    /// If value cannot be loaded, false is returned and value is set to `default_value`
    template <typename DECODER, typename T, typename DEFAULT>
    bool Load(const ConcreteAttributePath & path, T & value, const DEFAULT & default_value)
    {
        MutableByteSpan data = DECODER::StartLoad(value);
        if (!VerifySuccessLogOnFailure(path, mProvider.ReadValue(path, data)))
        {
            DECODER::LoadDefault(value, default_value);
            return false;
        }
        if (!DECODER::FinalizeLoad(value, data))
        {
            DECODER::LoadDefault(value, default_value);
            return false;
        }
        return true;
    }

    /// helper to not create a separate ShortPascalString out of a buffer.
    template <size_t N>
    CHIP_ERROR StorePascalString(const ConcreteAttributePath & path, AttributeValueDecoder & decoder, char (&buffer)[N])
    {
        Storage::ShortPascalString value(buffer);
        return Store<Persistence::String<Storage::ShortPascalString>>(path, decoder, value);
    }

    /// helper to not create a separate ShortPascalBytes out of a buffer.
    template <size_t N>
    CHIP_ERROR StorePascalString(const ConcreteAttributePath & path, AttributeValueDecoder & decoder, uint8_t (&buffer)[N])
    {
        Storage::ShortPascalBytes value(buffer);
        return Store<Persistence::String<Storage::ShortPascalBytes>>(path, decoder, value);
    }

private:
    AttributePersistenceProvider & mProvider;

    /// Help to debug attribute load errors.
    ///
    /// returns true if `err` is CHIP_NO_ERROR.
    /// Otherwise returns false and logs if  err is not CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND
    static bool VerifySuccessLogOnFailure(const ConcreteAttributePath & path, CHIP_ERROR err);
};

} // namespace chip::app
