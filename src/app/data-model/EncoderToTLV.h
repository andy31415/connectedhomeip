/*
 *    Copyright (c) 2020-2021 Project CHIP Authors
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

#include <app/data-model/Encode.h>
#include <lib/core/CHIPError.h>
#include <lib/core/TLV.h>

namespace chip {
namespace app {
namespace DataModel {

/// Defines an abstract class of something that can be encoded
/// into a TLV with a given data tag
class EncoderToTLV
{
public:
    virtual ~EncoderToTLV() = default;

    virtual CHIP_ERROR Encode(TLV::TLVWriter & writer, TLV::Tag tag) = 0;
};

/// An `EncoderToTLV` that uses `DataModel::Encode` to encode things.
///
/// Generally useful to encode things like <ClusterName>::Commands::<CommandName>::Type
/// structures.
template <typename T>
class ObjectEncoderToTLV : public EncoderToTLV
{
public:
    /// Encodes the given value via `DataModel::Encode` when the underlying
    /// encode is called.
    ///
    /// LIFETIME NOTE: uses a reference to value, so value must live longer than
    ///                this object.
    ObjectEncoderToTLV(const T & value) : mValue(value) {}

    CHIP_ERROR Encode(TLV::TLVWriter & writer, TLV::Tag tag) override { return DataModel::Encode(writer, tag, mValue); }

private:
    const T & mValue;
};


} // namespace DataModel
} // namespace app
} // namespace chip