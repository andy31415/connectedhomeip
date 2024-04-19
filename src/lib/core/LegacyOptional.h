/*
 *    Copyright (c) 2020-2024 Project CHIP Authors
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

#include <optional>
#include <type_traits>
#include <utility>

#include <lib/core/InPlace.h>
#include <lib/support/CodeUtils.h>

namespace chip {

/// An empty class type used to indicate optional type with uninitialized state.
struct NullLegacyOptionalType
{
    explicit NullLegacyOptionalType() = default;
};
inline constexpr NullLegacyOptionalType LegacyNullOptional{};

/**
 * Pairs an object with a boolean value to determine if the object value
 * is actually valid or not.
 *
 * NOTE: This class WILL be replaced in time with std::optional and currently exists
 *       for a transitional period only;
 */
template <class T>
class LegacyOptional
{
public:
    constexpr LegacyOptional() : mHasValue(false) {}
    constexpr LegacyOptional(NullLegacyOptionalType) : mHasValue(false) {}

    ~LegacyOptional()
    {
        // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch): mData is set when mHasValue
        if (mHasValue)
        {
            mValue.mData.~T();
        }
    }

    explicit LegacyOptional(const T & value) : mHasValue(true) { new (&mValue.mData) T(value); }

    template <class... Args>
    constexpr explicit LegacyOptional(InPlaceType, Args &&... args) : mHasValue(true)
    {
        new (&mValue.mData) T(std::forward<Args>(args)...);
    }

    constexpr LegacyOptional(const LegacyOptional & other) : mHasValue(other.mHasValue)
    {
        if (mHasValue)
        {
            new (&mValue.mData) T(other.mValue.mData);
        }
    }

    // Converts an LegacyOptional of an implicitly convertible type
    template <class U, std::enable_if_t<!std::is_same_v<T, U> && std::is_convertible_v<const U, T>, bool> = true>
    constexpr LegacyOptional(const LegacyOptional<U> & other) : mHasValue(other.has_value())
    {
        if (mHasValue)
        {
            new (&mValue.mData) T(other.Value());
        }
    }

    // Converts an LegacyOptional of a type that requires explicit conversion
    template <class U,
              std::enable_if_t<!std::is_same_v<T, U> && !std::is_convertible_v<const U, T> && std::is_constructible_v<T, const U &>,
                               bool> = true>
    constexpr explicit LegacyOptional(const LegacyOptional<U> & other) : mHasValue(other.has_value())
    {
        if (mHasValue)
        {
            new (&mValue.mData) T(other.Value());
        }
    }

    constexpr LegacyOptional(LegacyOptional && other) : mHasValue(other.mHasValue)
    {
        if (mHasValue)
        {
            new (&mValue.mData) T(std::move(other.mValue.mData));
            other.mValue.mData.~T();
            other.mHasValue = false;
        }
    }

    constexpr LegacyOptional & operator=(const LegacyOptional & other)
    {
        if (mHasValue)
        {
            mValue.mData.~T();
        }
        mHasValue = other.mHasValue;
        if (mHasValue)
        {
            new (&mValue.mData) T(other.mValue.mData);
        }
        return *this;
    }

    constexpr LegacyOptional & operator=(LegacyOptional && other)
    {
        if (mHasValue)
        {
            mValue.mData.~T();
        }
        mHasValue = other.mHasValue;
        if (mHasValue)
        {
            new (&mValue.mData) T(std::move(other.mValue.mData));
            other.mValue.mData.~T();
            other.mHasValue = false;
        }
        return *this;
    }

    /// Constructs the contained value in-place
    template <class... Args>
    constexpr T & Emplace(Args &&... args)
    {
        if (mHasValue)
        {
            mValue.mData.~T();
        }
        mHasValue = true;
        new (&mValue.mData) T(std::forward<Args>(args)...);
        return mValue.mData;
    }

    /** Make the optional contain a specific value */
    constexpr void SetValue(const T & value)
    {
        if (mHasValue)
        {
            mValue.mData.~T();
        }
        mHasValue = true;
        new (&mValue.mData) T(value);
    }

    /** Make the optional contain a specific value */
    constexpr void SetValue(T && value)
    {
        if (mHasValue)
        {
            mValue.mData.~T();
        }
        mHasValue = true;
        new (&mValue.mData) T(std::move(value));
    }

    /** Invalidate the value inside the optional. LegacyOptional now has no value */
    constexpr void ClearValue()
    {
        if (mHasValue)
        {
            mValue.mData.~T();
        }
        mHasValue = false;
    }

    /** Gets the current value of the optional. Valid IFF `HasValue`. */
    T & Value() &
    {
        VerifyOrDie(HasValue());
        return mValue.mData;
    }

    /** Gets the current value of the optional. Valid IFF `HasValue`. */
    const T & Value() const &
    {
        VerifyOrDie(HasValue());
        return mValue.mData;
    }

    /** Gets the current value of the optional if the optional has a value;
        otherwise returns the provided default value. */
    const T & value_or(const T & defaultValue) const { return HasValue() ? Value() : defaultValue; }

    /** Checks if the optional contains a value or not */
    constexpr bool HasValue() const { return mHasValue; }

    bool operator==(const LegacyOptional & other) const
    {
        return (mHasValue == other.mHasValue) && (!other.mHasValue || (mValue.mData == other.mValue.mData));
    }
    bool operator!=(const LegacyOptional & other) const { return !(*this == other); }
    bool operator==(const T & other) const { return HasValue() && Value() == other; }
    bool operator!=(const T & other) const { return !(*this == other); }

    std::optional<T> std_optional() const
    {
        VerifyOrReturnValue(HasValue(), std::nullopt);
        return std::make_optional(Value());
    }

    /** Convenience method to create an optional without a valid value. */
    static LegacyOptional<T> Missing() { return LegacyOptional<T>(); }

    /** Convenience method to create an optional containing the specified value. */
    template <class... Args>
    static LegacyOptional<T> Value(Args &&... args)
    {
        return LegacyOptional(InPlace, std::forward<Args>(args)...);
    }

private:
    bool mHasValue;
    union Value
    {
        Value() {}
        ~Value() {}
        T mData;
    } mValue;
};

template <class T>
constexpr LegacyOptional<std::decay_t<T>> MakeLegacyOptional(T && value)
{
    return LegacyOptional<std::decay_t<T>>(InPlace, std::forward<T>(value));
}

template <class T, class... Args>

constexpr LegacyOptional<T> MakeLegacyOptional(Args &&... args)
{
    return LegacyOptional<T>(InPlace, std::forward<Args>(args)...);
}

} // namespace chip
